#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// Memory size
#define MEM_SIZE 2000

// Memory bus message actions
#define MEM_WRITE 0
#define MEM_READ 1
#define MEM_KILL 2

// Used for memory messages that don't need a value
#define MEM_NULL 0

// Default value for all memory locations
#define MEM_NODATA 0

// Memory initialization status messages
#define MEM_READY 0
#define MEM_FAIL 1

// Memory permission modes. Used by the memory bus
#define MODE_USER 0
#define MODE_KERNEL 1

// Interrupt flags (none is no interrupt)
#define INTERRUPT_NONE 0
#define INTERRUPT_SYSCALL 1
#define INTERRUPT_TIMER 2

// Interrupt handler Addresses
#define ADDR_TIMER 1000
#define ADDR_SYSCALL 1500

// Struct to hold file descriptors for the two pipes used to communicate between the CPU and Memory processes.
// Also handles memory reading/writing permissions.
struct MemoryBus {
    int write_to_mem;
    int read_from_cpu;
    int write_to_cpu;
    int read_from_mem;
    unsigned short mode;
};

// Message format for memory bus messages
struct MemoryBusMessage {
    unsigned short action;
    unsigned short address;
    int value;
};

// Function declarations
void main_cpu(struct MemoryBus bus, int timer_period);
void main_memory(struct MemoryBus bus, char *program_path);
int memory_request(struct MemoryBus bus, unsigned short action, unsigned short address, int value);
int read_program(char *program_path, int memory[]);
int get_next_operand(struct MemoryBus bus, int *PC);
void push_stack(struct MemoryBus bus, int *stack_ptr, int item);
int pop_stack(struct MemoryBus bus, int *stack_ptr);

/**
 * Entry point for the program. Its only role is to setup communication and fork the CPU and Memory processes.
 */
int main(int argc, char *argv[]) {
    // Check argument count
    if (argc < 3) {
        printf("Usage: %s <program_file> <timer_period>\n", argv[0]);
        exit(1);
    }

    // Create two pipes for bidirectional communication between the CPU and Memory processes
    int pipefd_cpu[2], pipefd_mem[2];
    if (pipe(pipefd_cpu) == -1 || pipe(pipefd_mem) == -1) {
        printf("Failed to open memory bus pipes.\n");
        exit(1);
    }

    // Create the memory bus with above pipe pair
    struct MemoryBus bus;
    bus.write_to_mem = pipefd_cpu[1];
    bus.read_from_cpu = pipefd_cpu[0];
    bus.write_to_cpu = pipefd_mem[1];
    bus.read_from_mem = pipefd_mem[0];
    bus.mode = MODE_USER;

    // Fork the CPU and Memory processes
    int child_pid = fork();
    if (child_pid == 0) {
        main_memory(bus, argv[1]);
    } else {
        main_cpu(bus, atoi(argv[2]));
        // Ask the child to die
        memory_request(bus, MEM_KILL, MEM_NULL, MEM_NULL);
    }

    waitpid(child_pid, NULL, 0);
    return 0;
}

/**
 * This simulates the memory bus from the CPU to Memory. It can read, write, or kill the memory process.
 * It also prevents read/writing system memory when in user mode.
 * If action is MEM_KILL, the memory process will exit.
 */
int memory_request(struct MemoryBus bus, unsigned short action, unsigned short address, int value) {
    if (address >= MEM_SIZE / 2 && bus.mode == MODE_USER) {
        printf("Memory violation: accessing system address %d in user mode\n", address);
        exit(1);
    }

    struct MemoryBusMessage message = {action, address, value};
    int res = write(bus.write_to_mem, &message, sizeof(struct MemoryBusMessage));
    if (res == -1) {
        printf("CPU: Failed to write to memory bus.\n");
        exit(1);
    }

    int buffer;
    res = read(bus.read_from_mem, &buffer, sizeof(int));
    if (res == -1) {
        printf("CPU: Failed to read from memory bus.\n");
        exit(1);
    }

    return buffer;
}

/**
 * Entry point for the Memory-only child process. Effectively the main function of the Memory.
 * It can only do two things: read and write at specified memory addresses.
 */
void main_memory(struct MemoryBus bus, char *program_path) {
    // This is the emulator's main memory. The first half is user space, second half is system space
    // We want each location to be 0 initially so there aren't arbitrary instructions in unused memory
    int memory[MEM_SIZE] = {MEM_NODATA};

    // Read the source program into memory
    int err = read_program(program_path, memory);
    if (err != 0) {
        printf("MEMORY: (EC: %d) Failed to read program file: %s\n", err, program_path);
        int fail_message = MEM_FAIL;
        write(bus.write_to_cpu, &fail_message, sizeof(int));
        _exit(1);
    }

    // Signal CPU that memory is ready
    int ready_message = MEM_READY;
    write(bus.write_to_cpu, &ready_message, sizeof(int));

    // Listen for memory messages
    // Message = ACTION, ADDRESS, VALUE
    // This loop will only exit when MEM_KILL is given
    struct MemoryBusMessage message;
    for (;;) {
        if (read(bus.read_from_cpu, &message, sizeof(struct MemoryBusMessage)) == -1) {
            printf("MEMORY: Failed to read from memory bus.\n");
            _exit(1);
        }

        // Exit memory process
        if (message.action == MEM_KILL) {
            int empty_message = MEM_NULL;
            if (write(bus.write_to_cpu, &empty_message, sizeof(empty_message)) == -1) {
                printf("MEMORY: Failed to write to memory bus when exiting.\n");
                _exit(1);
            }
            _exit(0);
        }

        // Write to memory
        if (message.action == MEM_WRITE)
            memory[message.address] = message.value;

        // Send requested memory value back to CPU
        write(bus.write_to_cpu, &memory[message.address], sizeof(int));
    }
}

/**
 * Read the program located at program_path into memory (assumes MEM_SIZE length).
 * Returns 0 on success, the error code otherwise.
 */
int read_program(char *program_path, int *memory) {
    FILE *program_file = fopen(program_path, "r");
    if (program_file == NULL) return -1;

    char c;  // Character buffer for loop
    bool address_change = false;
    int mem_index = 0, err = 0;

    // Loop through every character until the end of the file
    // The stream will be skip around so this loop is not continuous
    while ((c = fgetc(program_file)) != EOF) {
        // Detect if the upcoming number is an address change
        if (c == '.') {
            address_change = true;
            continue;
        }

        // Skip any remaining non-digit characters
        if (c < '0' || c > '9') continue;

        // We need to go back one character because we just read the first character of a number
        if (fseek(program_file, -1L, SEEK_CUR) != 0) {
            err = -2;
            break;
        }

        // This gets the number and skips to the start of the next line
        int num;
        if (fscanf(program_file, "%d%*[^\n]%*[\n]", &num) != 1) {
            err = -3;
            break;
        }

        // Write to memory or change address
        if (address_change) {
            address_change = false;
            mem_index = num;
        } else {
            if (mem_index >= MEM_SIZE || mem_index < 0) {
                err = -4;
                break;
            }
            memory[mem_index] = num;
            mem_index++;
        }
    }

    fclose(program_file);
    return err;
}

/**
 * Entry point for the CPU-only process. Effectively the main function of the CPU.
 */
void main_cpu(struct MemoryBus bus, int timer_period) {
    // Registers
    int PC = 0,             // Program Counter
        SP = MEM_SIZE / 2,  // User Stack Pointer (dec then write)
        IR = 0,             // Instruction register
        AC = 0,             // Accumulator
        X = 0,              // Misc register
        Y = 0;              // Misc register

    // Temporary variables
    int timer_count = 0,  // Causes interrupt when equal to or above timer_period
        operand,          // Temporary operand holding variable
        SSP;              // System stack pointer

    // Keeps track of which interrupt the processor is handling to avoid nested ones
    short interrupt_flag = INTERRUPT_NONE;

    // Seed RNG for instructions that need it
    srand(time(NULL));

    // Wait for memory to read the program file and signal that it is ready
    int memory_ready_message;
    read(bus.read_from_mem, &memory_ready_message, sizeof(int));
    if (memory_ready_message != MEM_READY) {
        printf("CPU: Memory failed to start.\n");
        exit(1);
    }

    // Loop will only exit when the program calls the Exit instruction
    for (;;) {
        // Fetch instruction
        IR = memory_request(bus, MEM_READ, PC, MEM_NULL);

        // Process instruction
        switch (IR) {
            case 1:  // Load value
                AC = get_next_operand(bus, &PC);
                PC++;
                break;

            case 2:  // Load address
                operand = get_next_operand(bus, &PC);
                AC = memory_request(bus, MEM_READ, operand, MEM_NULL);
                PC++;
                break;

            case 3:  // LoadInd addr
                operand = get_next_operand(bus, &PC);
                operand = memory_request(bus, MEM_READ, operand, MEM_NULL);
                AC = memory_request(bus, MEM_READ, operand, MEM_NULL);
                PC++;
                break;

            case 4:  // LoadIdxX address
                operand = get_next_operand(bus, &PC);
                AC = memory_request(bus, MEM_READ, operand + X, MEM_NULL);
                PC++;
                break;

            case 5:  // LoadIdxY address
                operand = get_next_operand(bus, &PC);
                AC = memory_request(bus, MEM_READ, operand + Y, MEM_NULL);
                PC++;
                break;

            case 6:  // LoadSpX
                AC = memory_request(bus, MEM_READ, SP + X, MEM_NULL);
                PC++;
                break;

            case 7:  // Store address
                operand = get_next_operand(bus, &PC);
                memory_request(bus, MEM_WRITE, operand, AC);
                PC++;
                break;

            case 8:  // Get
                AC = rand() % 100 + 1;
                PC++;
                break;

            case 9:  // Put port
                operand = get_next_operand(bus, &PC);
                if (operand == 1)
                    printf("%d", AC);
                else if (operand == 2)
                    printf("%c", (char)AC);
                else {
                    printf("CPU: Invalid port value: %d\n", operand);
                    exit(1);
                }
                PC++;
                break;

            case 10:  // AddX
                AC += X;
                PC++;
                break;

            case 11:  // AddY
                AC += Y;
                PC++;
                break;

            case 12:  // SubX
                AC -= X;
                PC++;
                break;

            case 13:  // SubY
                AC -= Y;
                PC++;
                break;

            case 14:  // CopyToX
                X = AC;
                PC++;
                break;

            case 15:  // CopyFromX
                AC = X;
                PC++;
                break;

            case 16:  // CopyToY
                Y = AC;
                PC++;
                break;

            case 17:  // CopyFromY
                AC = Y;
                PC++;
                break;

            case 18:  // CopyToSp
                SP = AC;
                PC++;
                break;

            case 19:  // CopyFromSp
                AC = SP;
                PC++;
                break;

            case 20:  // Jump addr
                PC = get_next_operand(bus, &PC);
                break;

            case 21:  // JumpIfEqual addr
                operand = get_next_operand(bus, &PC);
                if (AC == 0)
                    PC = operand;
                else
                    PC++;
                break;

            case 22:  // JumpIfNotEqual addr
                operand = get_next_operand(bus, &PC);
                if (AC != 0)
                    PC = operand;
                else
                    PC++;
                break;

            case 23:  // Call addr
                operand = get_next_operand(bus, &PC);
                push_stack(bus, &SP, PC);
                PC = operand;
                break;

            case 24:  // Ret
                PC = pop_stack(bus, &SP);
                PC++;
                break;

            case 25:  // IncX
                X++;
                PC++;
                break;

            case 26:  // DecX
                X--;
                PC++;
                break;

            case 27:  // Push
                push_stack(bus, &SP, AC);
                PC++;
                break;

            case 28:  // Pop
                AC = pop_stack(bus, &SP);
                PC++;
                break;

            case 29:  // Int (System call)
                if (interrupt_flag != INTERRUPT_NONE) {
                    printf("CPU: No nested interrupts (attempted syscall during another interrupt)\n");
                    exit(1);
                }
                interrupt_flag = INTERRUPT_SYSCALL;
                bus.mode = MODE_KERNEL;
                SSP = MEM_SIZE;
                push_stack(bus, &SSP, PC + 1);  // +1 because we don't want to repeat this instruction
                push_stack(bus, &SSP, SP);
                SP = SSP;
                PC = ADDR_SYSCALL;
                break;

            case 30:  // IRet
                SSP = SP;
                SP = pop_stack(bus, &SSP);
                PC = pop_stack(bus, &SSP);
                bus.mode = MODE_USER;
                interrupt_flag = INTERRUPT_NONE;
                break;

            case 50:  // Exit
                return;

            // Error Cases:
            case MEM_NODATA:  // No Instruction
                if (PC == ADDR_SYSCALL) {
                    printf("CPU: Did syscall without an interrupt handler. No instruction at: %d\n", PC);
                } else if (PC == ADDR_TIMER) {
                    printf("CPU: Did timer interrupt without an interrupt handler. No instruction at: %d\n", PC);
                } else {
                    printf("CPU: No instruction at address %d\n", PC);
                }
                exit(1);

            default:  // Unknown, nonzero instruction
                printf("CPU: Unknown instruction: %d\n", IR);
                exit(1);
        }

        // Timer interrupt
        if (interrupt_flag == INTERRUPT_NONE && timer_count >= timer_period) {
            timer_count = 0;
            interrupt_flag = INTERRUPT_TIMER;
            bus.mode = MODE_KERNEL;
            SSP = MEM_SIZE;
            push_stack(bus, &SSP, PC);
            push_stack(bus, &SSP, SP);
            SP = SSP;
            PC = ADDR_TIMER;
        }

        // Increment the timer count, but throw an error if it will cause an infinite series of interrupts
        timer_count++;
        if (interrupt_flag == INTERRUPT_TIMER && timer_count >= timer_period) {
            printf("CPU: Timer handler exceeded timer period, resulting in an infinite loop. Aborted.\n");
            exit(1);
        }
    }
}

/**
 * Retrieves the next operand from memory and increments the program counter in place.
 */
int get_next_operand(struct MemoryBus bus, int *PC) {
    return memory_request(bus, MEM_READ, ++(*PC), MEM_NULL);
}

/**
 * Decrements a stack pointer in place then pushes an item into the new address
 */
void push_stack(struct MemoryBus bus, int *stack_ptr, int item) {
    memory_request(bus, MEM_WRITE, --(*stack_ptr), item);
}

/**
 * Reads an item at a stack pointer address then increments it in place.
 */
int pop_stack(struct MemoryBus bus, int *stack_ptr) {
    return memory_request(bus, MEM_READ, (*stack_ptr)++, MEM_NULL);
}
