## Project Purpose

The primary purpose of this project is to create a doctor office simulation exclusively using semaphores. Different personnel within the doctor's office each operate their own threads, so managing their interactions is a difficult but enlightening task.

The four key people involved in this simulation are patients, a receptionist, doctors, and nurses. A receptionist checks in patients, nurses lead patients to the doctor they are assigned to, and doctors consult with patients before they leave.

## Implementation

This project is implemented in Java.

Each person in the simulation is represented by a class. The main classes are `Patient`, `Receptionist`, `Nurse`, and `Doctor`. These classes all inherit from `Runnable`, so instances of these classes act as individual threads.

There is also a global context class `OfficeContext` that initializes semaphores and stores global variables. An instance of this class is shared among all threads.

There are also a few helper classes to assist with concurrency operations: `MutexValue`, `OfficeQueue`, and `RemainingItemsTracker`.

Further implementation and design considerations are described in the design document.

## Personal Experience

Unlike project 1, I chose Java for this project because I really didn't want to work with `pthreads`. I believe this was a wise choice for my sanity.

At the beginning, I was able to create all basic thread classes and run them easily. The difficult part was having them interact correctly. Looking at the barbershop example, I knew I would need a queue to keep the check-in system fair and allow the receptionist to print the patient ID. Specifically, a mutex would be used for enqueuing and dequeuing, a counting semaphore would be used to track new items, and a binary semaphore array would be used to wait for a specific item to be processed. I also realized a queue could also be used for nurses.

However, I naively applied the queue technique to the doctor threads as well. I continued developing this way until I eventually realized that doctors were seeing the wrong patients. This was because patients were added to a single queue after being seen by a nurse, meaning any doctor could dequeue them. This caused me to drastically rework my solution.

Eventually, I realized that according to the project requirements, nurses and doctors are mutually exclusive if they share an ID. This is because a nurse cannot send a patient to a doctor that is busy. Furthermore, a patient cannot do anything while a doctor or nurse is handling them. Thus, in this context, all three are mutually exclusive since only one of them can run at a time.

This realization allowed me to fix my solution using a few semaphore arrays that lock the other two threads while one is processing. Arrays were necessary since the threads are only mutually exclusive if the nurse and doctor share an ID and if the patient was assigned to them. Although I personally find this solution less elegant than the queue-based approach (or any other approach that uses fewer semaphores), it works and is simple enough.

Another problem I had was detecting when a thread could exit. This was not a problem for the receptionist since there is only one thread for it, so it could just count until it saw every patient. However, for doctor and nurse threads, each thread will see a different amount of patients. This means that it is difficult to keep track of how many more patients will need to be seen. Furthermore, once a thread starts waiting for a new patient, it cannot stop. 

Thankfully, I eventually realized that if the number of remaining patients was less than the number of threads, any extra threads can quit. This led me to developing a new class with a can-I-quit function that returns true when the number of running threads is more than the remaining items. These numbers can also be updated whenever a patient is processed and whenever a thread quits. A thread can also call this function prior to waiting, so it can quit at the perfect time. This solution worked perfectly, and I think this solution is elegant.

Overall, I found this project more difficult than project 1. I was not expecting this, and it makes me even more glad I wrote it in Java and not C/C++. However, I still think it was an enlightening and honestly pretty fun experience.
