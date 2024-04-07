# Project Purpose

The primary purpose of this project is to create a doctor office simulation exclusively using Semaphores. Different personnel within the doctors office each operate their own threads, so managing their interactions is a difficult but enlightening task.

The four key people involved in this simulation are patients, a receptionist, doctors, and nurses.

## Implementation

This project is implemented in Java.

Each person in the simulation is represented by a class. The main classes are `Patient`, `Receptionist`, `Nurse`, and `Doctor`. Instances of these classes act as individual threads.

There is also a global context class `OfficeContext` that initializes semaphores and stores global variables. An instance of this class is shared among all threads.

There are also a few helper classes to assist with concurrency operations.

A class named `MutexValue` is used to manage values that are shared among multiple threads. Essentially, it is a wrapper around a value that can be locked and unlocked using a binary semaphore. As an example, it is used for locking/unlocking the checkin queue.

Queues are implemented using the `OfficeQueue` class. This class has a mutex value that protects a linked list queue. Whenever an item is enqueued, a counting semaphore representing the number of unprocessed items in the queue is incremented. This lets consumers run when there are new items in the queue. There is also an array of semaphores that allow producers to wait until their produced item is processed. This is useful for, as an example, when a patient enters the checkin queue and needs to know when they can sit back down.

Finally, there is a `RemainingItemsTracker` class that is used by the doctor and nurse threads to know when to exit. Unlike the receptionist thread that knows it will see every patient, some doctor/nurse threads will see differing number of patients. This class allows them to keep track of how many more patients they could see, and if the remaining number is less than the number of threads, they know they can exit. This is implemented via a pair of integers with a corresponding mutex semaphore. One integer represents the number of threads still working, and the other is the number of items remaining.

## Personal Experience

Unlike project 1, I chose Java for this project because I really didn't want to work with pthreads. I believe this was a wise choice for my sanity.

I was able to create all basic thread classes and run them easily. The difficult part was having them interact correctly. Looking at the barbershop example, I knew I would need a queue to keep the check-in system fair and allow the receptionist to print the patient ID. I also realized a queue could also be used for nurses.

However, I naively applied the queue technique to the doctor threads as well. I continued developing this way until I eventually realized that doctors were seeing the wrong patients. This was because patients were added to a single queue after being seen by a nurse, meaning any doctor could dequeue them. This caused me to drastically rework my solution.

Eventually, I realized that according to the project requirements, nurses and doctors are mutually exclusive if they share an ID. This is because a nurse cannot send a patient to a doctor that is busy. Furthermore, a patient cannot do anything while a doctor or nurse is handling them. Thus, in this case, all three are mutually exclusive.

This realization allowed me to fix my solution using a few semaphore arrays that lock the other two threads while one is processing. Although I personally find this solution less elegant than the queue-based approach (or any other approach that uses fewer semaphores), it works and is simple enough.

Overall, I found this project more difficult than project 1. I was not expecting this, and it makes me even more glad I wrote it in Java and not C/C++.
