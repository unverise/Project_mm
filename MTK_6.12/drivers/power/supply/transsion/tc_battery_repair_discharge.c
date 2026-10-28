#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/atomic.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/sched.h>    // For struct sched_param and scheduling functions
#include <linux/sched/rt.h> // For real-time scheduling in kernel
#include <linux/vmalloc.h>
#define DISCHARGE_STOP 0          // State: Stop discharge
#define DISCHARGE_SMALL 1         // State: Small current discharge
#define DISCHARGE_LARGE_CPU 2         // State: Large current discharge
#define DISCHARGE_LARGE_DRAM 3         // State: Larger current discharge
#define MAX_THREADS 8            // Maximum number of threads supported
#define DDR_BUFFER_SIZE (1024 * 1024 * 256)  // 256MB DDR buffer (adjust based on 4GB total RAM)
#define DDR_ACCESS_ITERATIONS 2000000        // Increased iterations for maximum load
#define DDR_BURST_SIZE 128        // Optimized burst size for most DDR controllers
#define PARALLEL_DDR_THREADS 8     // Number of threads for parallel DDR access
// Global DDR buffer pointer
static char *ddr_buffer = NULL;
// Flag to track if buffer is allocated
static bool ddr_buffer_allocated = false;
// Atomic variable to store the current discharge state
static atomic_t discharge_state = ATOMIC_INIT(DISCHARGE_STOP);
static atomic_t ddr_stop_flag = ATOMIC_INIT(0);
// Array to hold pointers to the discharge threads
static struct task_struct *discharge_threads[MAX_THREADS] = {NULL};
// Mutex lock for thread management to ensure thread safety
static struct mutex thread_lock;
// Kernel object class for exposing sysfs interfaces
static struct class *battery_class;
// Function to allocate DDR buffer if needed
static int allocate_ddr_buffer(void) {
    mutex_lock(&thread_lock);
    if (ddr_buffer_allocated && ddr_buffer) {
        mutex_unlock(&thread_lock);
        return 0; // Buffer already allocated
    }
    // Allocate large DDR buffer (256MB) for maximum bandwidth utilization
    ddr_buffer = vmalloc(DDR_BUFFER_SIZE);
    if (!ddr_buffer) {
        pr_err("Failed to allocate DDR buffer\n");
        mutex_unlock(&thread_lock);
        return -ENOMEM;
    }else{
        ddr_buffer_allocated = true;
        pr_info("Allocated %luMB DDR buffer for load simulation\n", (unsigned long)DDR_BUFFER_SIZE / (1024 * 1024));
    }
    mutex_unlock(&thread_lock);
    return 0;
}

// Function to free DDR buffer
static void free_ddr_buffer(void) {
    mutex_lock(&thread_lock);
    if (ddr_buffer_allocated && ddr_buffer) {
        vfree(ddr_buffer);
        ddr_buffer = NULL;
        ddr_buffer_allocated = false;
        pr_info("Freed DDR buffer\n");
    }
    mutex_unlock(&thread_lock);
}

// **Simulated discharge load**
static struct task_struct *ddr_threads[PARALLEL_DDR_THREADS] = {NULL};
// This function performs optimized memory access to maximize DDR bandwidth utilization
static void simulate_ddr_load(void) {
    // Allocate buffer if not already allocated
    if (!ddr_buffer_allocated) {
        if (allocate_ddr_buffer() != 0) {
            pr_err("Failed to allocate DDR buffer for simulation\n");
            return;
        }
    }
    if (atomic_read(&ddr_stop_flag)) {
        return;
    }
    // Calculate safe buffer size limit
    size_t safe_buffer_limit = DDR_BUFFER_SIZE - DDR_BURST_SIZE;
    // Use burst access pattern optimized for DDR controllers
    for (int i = 0; i < DDR_ACCESS_ITERATIONS; i++) {
        if ((i % 1000) == 0) {
            if (atomic_read(&ddr_stop_flag)) {
                return;
            }
        }
        // Generate safe random offset using kernel's random number API
        // This ensures no integer overflow and provides better randomization
        size_t offset = get_random_u32() % safe_buffer_limit;
        volatile char *ptr = &ddr_buffer[offset];
        // Write burst
        for (int j = 0; j < DDR_BURST_SIZE; j++) {
            ptr[j] = (char)(i + j);
        }
        // Read burst and perform computation to prevent optimization
        volatile int sum = 0;
        for (int j = 0; j < DDR_BURST_SIZE; j++) {
            sum += ptr[j];
        }
    }
}
// **DDR thread function**
// Dedicated thread for DDR load generation
static int ddr_thread_fn(void *data) {
    unsigned long thread_id = (unsigned long)data;
    pr_info("DDR thread %lu started\n", thread_id);
    // Main loop for the thread
    while (!kthread_should_stop()) {
        // Read the current discharge state atomically
        int current_state = atomic_read(&discharge_state);
        // Perform discharge simulation only if the state is SMALL or LARGE
        if (current_state != DISCHARGE_STOP) {
            simulate_ddr_load();
        }
    }
    pr_info("DDR thread %lu exiting\n", thread_id);
    return 0;
}
// A function to simulate computational load based on the discharge type.
// This is used to emulate power consumption during battery discharge.
static void cpu_simulate_discharge(int type) {
    int i, j;
    const int scale = 111111; // Fixed-point scaling factor for simulation
    // Perform a large number of iterations to simulate CPU load
    for (i = 0; i < 200000; i++) {
        volatile int x = i * scale;       // Simulate integer multiplication
        for (j = 0; j < 10; j++) {        // Nested loops to increase computation complexity
            x = (x * j) / 27180;          // Simulate division, approximately 2.718
            x = (x * x) / scale;          // Simulate squaring operation
        }
        x = x + 14140;                    // Simulate addition, approximately 1.414
    }
    // For small current discharge, introduce a sleep to reduce CPU load
    if (type == DISCHARGE_SMALL) {
        msleep(10); // Sleep for 10 millisecond
    }else{
        msleep(5);
    }
}
// **Thread function for discharge**
// Each thread executes this function to perform discharge simulation based on the current state.
// Threads will terminate when `kthread_should_stop()` returns true.
static int cpu_discharge_thread_fn(void *data) {
    unsigned long thread_id = (unsigned long)data; // Thread ID passed during thread creation
    pr_info("Discharge thread %lu started\n", thread_id);
    // Main loop for the thread
    while (!kthread_should_stop()) {
        // Read the current discharge state atomically
        int current_state = atomic_read(&discharge_state);
        // Perform discharge simulation only if the state is SMALL or LARGE
        if (current_state != DISCHARGE_STOP) {
            cpu_simulate_discharge(current_state);
        }
    }
    pr_info("Discharge thread %lu exiting\n", thread_id);
    return 0; // Thread exits here
}
// **Start thread pool for discharge**
// This function starts the specified number of threads for performing discharge simulation.
static void start_cpu_threads(int thread_count) {
    int i;
    // Use a mutex to ensure thread-safe operations
    mutex_lock(&thread_lock);
    // Loop to create threads up to the requested count
    for (i = 0; i < thread_count; i++) {
        // Start a thread only if it doesn't already exist
        if (!discharge_threads[i]) {
            discharge_threads[i] = kthread_run(cpu_discharge_thread_fn, (void *)(unsigned long)i, "discharge_thread_%d", i);
            if (IS_ERR(discharge_threads[i])) {
                pr_err("Failed to start discharge thread %d\n", i);
                discharge_threads[i] = NULL;
            } else {
                pr_info("Started discharge thread %d\n", i);
            }
        }
        msleep(5); // Sleep for 5 millisecond
    }
    mutex_unlock(&thread_lock); // Release the lock after creating threads
}
// **Start DDR thread pool**
// This function starts dedicated threads for DDR load generation
static void start_ddr_threads(int thread_count) {
    if (allocate_ddr_buffer() != 0) {
        pr_err("Failed to allocate DDR buffer for simulation\n");
        return;
    }
    int i;
    atomic_set(&ddr_stop_flag, 0);
    mutex_lock(&thread_lock);
    for (i = 0; i < thread_count; i++) {
        if (!ddr_threads[i]) {
            ddr_threads[i] = kthread_run(ddr_thread_fn, (void *)(unsigned long)i, "discharge_ddr_thread_%d", i);
            if (IS_ERR(ddr_threads[i])) {
                pr_err("Failed to start discharge ddr thread %d\n", i);
                ddr_threads[i] = NULL;
            } else {
                pr_info("Started discharge ddr thread %d\n", i);
            }
        }
        msleep(5); // Sleep for 5 millisecond
    }
    mutex_unlock(&thread_lock);
}
// **Stop all DDR threads**
// This function stops and cleans up all running DDR threads.
static void stop_ddr_threads(void) {
    int i;
    int old_nice = task_nice(current);
    // Set current process to highest real-time priority using proper API
    set_user_nice(current, MAX_RT_PRIO - 1 - 20);
    atomic_set(&ddr_stop_flag, 1);
    // Use original mutex lock mechanism
    mutex_lock(&thread_lock);
    for (i = 0; i < PARALLEL_DDR_THREADS; i++) {
        if (ddr_threads[i]) {
            pr_info("Stopping DDR discharge thread %d\n", i);
            kthread_stop(ddr_threads[i]);
            ddr_threads[i] = NULL;
        }
    }
    mutex_unlock(&thread_lock);
    // Restore original priority using proper API
    set_user_nice(current, old_nice);
    free_ddr_buffer();
}
// **Stop all threads in the thread pool**
// This function stops and cleans up all running discharge threads.
static void stop_cpu_threads(void) {
    int i;
    int old_nice = task_nice(current);
    // Set current process to highest real-time priority using proper API
    set_user_nice(current, MAX_RT_PRIO - 1 - 20);
    // Use original mutex lock mechanism
    mutex_lock(&thread_lock);
    // Loop to stop all threads
    for (i = 0; i < MAX_THREADS; i++) {
        if (discharge_threads[i]) {
            pr_info("Stopping CPU discharge thread %d\n", i);
            kthread_stop(discharge_threads[i]);
            discharge_threads[i] = NULL;
        }
    }
    mutex_unlock(&thread_lock);
    // Restore original priority using proper API
    set_user_nice(current, old_nice);
}
// **Write interface: switch discharge state based on input**
// This function is triggered when a value is written to the sysfs file.
// It updates the discharge state and adjusts the thread pool accordingly.
static ssize_t discharge_store(const struct class *class, const struct class_attribute *attr,
                                  const char *buf, size_t count) {
    int state;
    // Parse the input value as an integer
    if (kstrtoint(buf, 10, &state)) {
        pr_err("Invalid input: %s\n", buf);
        return -EINVAL; // Return error if conversion fails
    }
    // Validate the input state
    if (state != DISCHARGE_STOP && state != DISCHARGE_SMALL && state != DISCHARGE_LARGE_CPU && state != DISCHARGE_LARGE_DRAM) {
        pr_err("Invalid discharge state: %d\n", state);
        return -EINVAL; // Return error for invalid state
    }
    pr_info("Changing discharge state to %d\n", state);
    // Update the atomic discharge state
    atomic_set(&discharge_state, state);
    // Adjust the thread pool based on the new state
    if (state == DISCHARGE_STOP) {
        stop_cpu_threads();
        stop_ddr_threads();
    } else if (state == DISCHARGE_SMALL) {
        stop_cpu_threads();
        stop_ddr_threads();
        msleep(5);
        start_cpu_threads(4); // Start 4 threads for small current discharge
    } else if (state == DISCHARGE_LARGE_CPU) {
        stop_cpu_threads();
        stop_ddr_threads();
        msleep(10);
        start_ddr_threads(4);
        start_cpu_threads(4); // Start 4 threads for large current discharge
    } else if (state == DISCHARGE_LARGE_DRAM) {
        stop_cpu_threads();
        stop_ddr_threads();
        msleep(10);
        start_cpu_threads(8); // Start 8 threads for large current discharge
        start_ddr_threads(8); // PARALLEL_DDR_THREADS 8 Start 8 threads
    }
    return count; // Return the number of bytes written
}
// **Read interface: return the current discharge state**
// This function is triggered when the sysfs file is read.
// It returns the current value of the discharge state.
static ssize_t discharge_show(const struct class *class, const struct class_attribute *attr,
                                 char *buf) {
    int state = atomic_read(&discharge_state); // Atomically read the current state
    pr_info("Reading discharge state: %d\n", state);
    return scnprintf(buf, PAGE_SIZE, "%d\n", state); // Write the state to the buffer
}
// Define sysfs attributes for the discharge state
static CLASS_ATTR_RW(discharge);
// **Module initialization function**
// This function is called when the module is loaded into the kernel.
static int __init battery_repair_init(void) {
    int ret;
    // Initialize the mutex for thread management
    mutex_init(&thread_lock);
    // Create a kernel class for sysfs
    battery_class = class_create("battery_repair");
    if (IS_ERR(battery_class)) {
        pr_err("Failed to create class\n");
        vfree(ddr_buffer);  // Free DDR buffer
        ddr_buffer = NULL;
        return PTR_ERR(battery_class);
    }
    // Create a sysfs file for the discharge attribute
    ret = class_create_file(battery_class, &class_attr_discharge);
    if (ret) {
        pr_err("Failed to create sysfs file\n");
        class_destroy(battery_class);
        vfree(ddr_buffer);  // Free DDR buffer
        ddr_buffer = NULL;
        return ret;
    }
    pr_info("Battery repair module initialized\n");
    return 0; // Initialization successful
}
// **Module exit function**
// This function is called when the module is removed from the kernel.
static void __exit battery_repair_exit(void) {
    stop_cpu_threads(); // Stop all threads
    stop_ddr_threads();       // Stop all DDR threads
    class_remove_file(battery_class, &class_attr_discharge); // Remove sysfs file
    class_destroy(battery_class);                           // Destroy the kernel class
    pr_info("Battery repair module exited\n");
}
module_init(battery_repair_init); // Register the module initialization function
module_exit(battery_repair_exit); // Register the module exit function
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Shang Huihui");
MODULE_DESCRIPTION("Battery Repair Discharge Driver with Multi-threading");
