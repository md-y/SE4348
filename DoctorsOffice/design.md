## Design Overview

Since the program is implemented in Java, it is designed with OOP principles in mind. This means that every person in the doctor's office (patients, the receptionist, doctors, and nurses) are represented as classes where each instance is a thread.

However, since all these threads must communicate with each other, there is a stand-alone context class that stores all semaphores and other global data for these classes.

In the actual implementation, these person classes are `Patient`, `Receptionist`, `Nurse`, and `Doctor`. These classes inherit `Runnable` so they can be run as threads. The `OfficeContext` that has the role of initializing semaphores and storing global variables.

There are also a few helper classes that help manage semaphore operations, such as a `MutexValue` class, a `OfficeQueue` class for managing queues concurrently, and a `RemainingItemsTracker` that keeps track of the remaining tasks for each entity. Of course, these classes exclusively use semaphores.

`MutexValue` is used to manage values that are shared among multiple threads. Essentially, it is a wrapper around a value that can be locked and unlocked using a binary semaphore. As an example, it is used for locking/unlocking the check in queue.

Queues are implemented using the `OfficeQueue` class. This class has a mutex value that protects a linked list queue. Whenever an item is enqueued, a counting semaphore representing the number of unprocessed items in the queue is incremented. This lets consumers run when there are new items in the queue. There is also an array of semaphores that allow producers to wait until their produced item is processed. This is useful for, as an example, when a patient enters the check in queue and needs to know when they can sit back down.

Finally, there is a `RemainingItemsTracker` class that is used by the nurse threads (and doctors) to know when to exit. Unlike the receptionist thread that knows it will see every patient, some doctor/nurse threads will see differing number of patients. This class allows them to keep track of how many more patients they could see, and if the remaining number is less than the number of threads, they know they can exit. This is implemented via a pair of integers with a corresponding mutex semaphore. One integer represents the number of threads still working, and the other is the number of items remaining.

For brevity and simplicity, the following semaphore list and pseudocode mostly ignore these implementation definitions and instead focus on core logic. However, everything from these classes is still included.

## Semaphores

The following semaphores were used to manage interactions between patients and each other entity (the receptionist, nurses, and doctors). Because of this, each semaphore can be categorized into which non-patient person needs it.

#### Receptionist Semaphores

The patient check in processes needs to use a queue, so the receptionist can print the patient ID in a FIFO manner. This requires three semaphores: the queue mutex, a counting semaphore for the new items, and an array of binary semaphores for the patients to wait on.

These are for the receptionist queue (part of `OfficeQueue` instance in code):

```java
// Mutex semaphore for the check-in queue
receptionistQueue.queue = 1

// Counting semaphore for the number of waiting items
receptionistQueue.waitingSize = 0

// Binary semaphore array used to signal patients they are checked-in
receptionistQueue.waitingProducers = "all 0s array of size patient_count"
```

#### Nurse Semaphores

The nurse also has a queue for patients, but the patients are added by the receptionist since they are in charge of notifying the nurse (as per the requirements). The nurse has the similar semaphores for this.

These are for the nurse queue (part of `OfficeQueue` instance in code):

```java
// Mutex semaphore for queue
nurseQueue.queue = 1

// Counting semaphore for the number of waiting items
nurseQueue.waitingSize = 0

// Binary semaphores used to signal patients they have been led by a nurse
nurseQueue.waitingProducers = "all 0s array of size patient_count"
```

The nurse also needs to know when to quit the thread. A thread is able to quit when there are more nurse threads than unprocessed patients. This can be implemented using two numbers (remaining items and worker count) with one associated mutex.

This is for the nurse tracker (part of `RemainingItemsTracker` in code):

```java
// Mutex for remaining items and worker count tuple
nurseTracker.remaining = 1
```

There are two more semaphore arrays needed. One is for the nurse to wait on a specific patient to return to the waiting room before being led off. Without this, the nurse could lead away a patient, and then the patient says they enter the waiting room. The other is to signal to the doctor that the nurse is ready.

```java
// Binary semaphores used to signal nurses that a patient is ready to be led
patientsWaitingForNurse = "all 0s array of size patient_count"

// Binary semaphores used to signal doctors that the nurse is ready
readyNurses = "all 0s array of size doctor_count"
```

#### Doctor Semaphores

For any pair of doctors and nurses with the same ID, they are effectively mutually exclusive. This is because a nurse cannot give another patient to a doctor while they are working, and a doctor cannot do anything until a nurse gives them a patient. Furthermore, while a patient is waiting to be seen, they cannot do anything. This means these three people need to wait on each other based on their IDs. Because of this, arrays of semaphores are optimal since a thread can wait on a specific semaphore based on an integer ID.

```java
// Binary semaphores used to make doctors wait until the patient signals they are ready:
doctorsWaitingForPatient = "all 0s array of size doctor_count"

// Binary semaphores used to signal a nurse that their associated doctor is ready:
readyDoctors = "all 0s array of size doctor_count"

// Binary semaphores used by patients to wait for their doctor to be finished:
doctorFinished = "all 0s array of size doctor_count"

// Binary semaphores used by doctors to wait until the patient has left before resetting:
patientHasLeft = "all 0s array of size doctor_count"
```

## Pseudocode

The following pseudocode represent a more functional version of the OOP implementation to reflect the format of the textbook (as per the project requirements). All core logic is the same, and it follows the real implementation closely.

#### Main (Entry)

The main function is simply in charge of checking the arguments, creating the global state, and handling the threads.

```text
main(args) {
  check if args >= 2

  doctor_count = args[0]
  patient_count = args[1]

  check if 0 < doctor_count <= 3
  check if 0 < patient_count <= 15

  state = create global state object that holds state for all threads

  threads = thread array for all runnables
  add receptionist to thread array
  add doctors to thread array
  add patients to thread array

  for each thread in threads, start

  for each thread in threads, join
}
```

#### Global State Object

This object is shared between all threads. It creates semaphores exactly as described [[#Semaphores|above]] as well as a few non-semaphore shared variables.

```
init() {
  initialize all semaphores as stated at the top of the document

  // These arrays are needed since patients, nurses, and doctors need
  // to know specifically who they are interacting with.
  // They are indexed by patient IDs and doctor IDs.
  assignedDoctor = int[patientCount]
  assignedPatient = int[doctorCount]

  // Can be any implementation of the queue data structure
  receptionistQueue = new queue for patients
  nurseQueue = new queue for patients
}
```

#### Patient code

This is the most important function since the patient threads are in charge of directing the whole simulation.

```
run() {
  print entering message

  // Use mutex to enqueue
  wait(receptionistQueue.queue)
  enqueue(receptionistQueue, self)
  signal(receptionistQueue.queue)

  // Increment queue semaphore to signal new item
  signal(receptionistQueue.waitingSize)

  wait(receptionistQueue.waitingProducers[id])

  print leaving message

  signal(patientsWaitingForNurse[id])
  wait(nurseQueue.waitingProducers[id])

  // Need to keep same nurse/doctor id
  doctorId = assignedDoctor[id]

  print entering doctor office message

  signal(doctorsWaitingForPatient[doctorId])
  wait(doctorFinished[doctorId])

  print doctor advice message

  print leaving message
  signal(patientHasLeft[doctorId])
}
```

#### Receptionist code

Since there is only one receptionist, this function uses a for loop based on the patient count.

```
run() {
  for each patient {
    wait(receptionistQueue.waitingSize)

    wait(receptionistQueue.queue)
    patient = dequeue(receptionistQueue)
    signal(receptionistQueue.queue)

    print register message for patient

    signal(receptionistQueue.waitingProducers[patient])

    // This tells a nurse that there is a new patient
    wait(nurseQueue.queue)
    enqueue(nurseQueue, patient)
    signal(nurseQueue.queue)
  }
}
```

#### Nurse code

Nurses make use of a `tryToQuit` function that uses a pair of integers and an associated mutex to track the remaining number of patients and currently running nurse threads. This function is also described below.

```
nurseTracker = [remainingPatients, remainingNurses]
quitNurses = new bool[doctor_count]

tryToQuit() {
  canQuit = false

  // Remaining is the mutex for nurseTracker
  wait(nurseTracker.remaining)

  if remainingNurses > remainingPatients {
    remainingNurses--
    canQuit = true
  }
  signal(nurseTracker.remaining)
  return canQuit
}

run() {
  while tryToQuit() is false {
    signal(readyNurses[id])
    wait(readyDoctors[id])

    wait(nurseQueue.waitingSize)
    wait(nurseQueue.queue)
    patient = dequeue(nurseQueue)
    signal(nurseQueue.queue)

    wait(patientsReadyForNurse[id])

    print patient led to office message

    // This tells the doctor which patient they are waiting
    // for, and tells the patient which doctor they have.
    // These also don't need mutexes since assignedDoctor
    // is only written once per index, and the index
    // for assignedPatient is only written by this thread
    assignedDoctor[patient.id] = id
    assignedPatient[id] = patient.id

    signal(nurseProcessed[patient.id])

    wait(nurseTracker.remaining)
    remainingPatients--
    signal(nurseTracker.remaining)
  }

  quitNurses[id] = true
  signal(readyNurses[id])
}
```

#### Doctor Code

Doctors rely on their nurse quitting before they do.

```

tryToQuit() {
  wait(readyNurses[id])
  return quitNurses[id]
}

run() {
  while tryToQuit() is false {
    signal(readyDoctors[id])
    wait(doctorsWaitingForPatient[id])

    patient = assignedPatient[id]

    print patient symptoms message

    signal(doctorFinished[patient.id])
    wait(patientHasLeft[id])

    wait(doctorTracker.remaining)
    remainingPatients--
    signal(doctorTracker.remaining)
  }
}
```
