# Overview

Since the program is implemented in Java, it was designed with OOP principles in mind. The main classes are `Patient`, `Receptionist`, `Nurse`, and `Doctor`. Instances of these classes act as individual threads.

All instances share a common state, so there is another class `OfficeContext` that has the role of initializing semaphores and storing global variables.

There are also a few helper classes that help manage semaphore operations, such as a `MutexValue` class, a `OfficeQueue` class for managing queues, and a `RemainingItemsTracker` that keeps track of the remaining tasks for each entity.

However, for brevity and simplicity, the following semaphore list and pseudocode mostly ignore these implementation definitions and instead focus on core logic.

## Semaphores

The following semaphores were used to manage interactions between patients and each other entity (the receptionist, nurses, and doctors).

### Receptionist Semaphores

These are for the receptionist queue (part of `OfficeQueue` instance in code):

```java
// Mutex semaphore for the check-in queue
receptionistQueue.queue = 1

// Semaphore counting the number of waiting items
receptionistQueue.waitingSize = 0

// Semaphore array used to signal patients they are checked-in
receptionistQueue.waitingProducers = "all 0s array of size patient_count"
```

### Nurse Semaphores

These are for the nurse queue (part of `OfficeQueue` instance in code):

```java
// Mutex semaphore for queue
nurseQueue.queue = 1

// Semaphore counting the number of waiting items
nurseQueue.waitingSize = 0

// Semaphores used to signal patients they have been led by a nurse
nurseQueue.waitingProducers = "all 0s array of size patient_count"
```

These are for the nurse tracker (part of `RemainingItemsTracker` in code):

```java
// Mutex for remaining items and worker count tuple
nurseTracker.remaining = 1
```

These are other semaphores for nurses:

```java
// Semaphores used to signal nurses that a patient is ready to be led
patientsWaitingForNurse = "all 0s array of size patient_count"
```

### Doctor Semaphores

These are for handling the nurse-to-doctor hand-off:

```java
// Semaphores used to make doctors wait until the patient signals they are ready:
doctorsWaitingForPatient = "all 0s array of size doctor_count"

// Used to signal a nurse that their associated doctor is ready:
readyDoctors = "all 0s array of size doctor_count"

// Used by patients to wait for their doctor to be finished:
doctorFinished = "all 0s array of size doctor_count"

// Used by doctors to wait until the patient has left before resetting:
patientHasLeft = "all 0s array of size doctor_count"
```

These are for the doctor tracker (part of `RemainingItemsTracker` in code):

```java
// Mutex for remaining items and worker count tuple
doctorTracker.remaining = 1
```

## Pseudocode

The following pseudocode assumes the semaphores above were already defined during initialization, and that they are accessible by all methods.

### Patient code

```
run() {
  print entering message

  wait(mutex for receptionistQueue)
  enqueue(self) to receptionistQueue
  signal(mutex for receptionistQueue)
  signal(receptionistQueue)

  wait(checkIn[id])

  print leaving message

  signal(readyForNurse[id])
  wait(nurseProcessing[id])

  // Need to keep same nurse/doctor id
  doctorId = assignedDoctor[id]

  print entering doctor office message

  signal(readyForDoctor[doctorId])
  wait(doctorFinished[doctorId])

  print doctor advice message

  print leaving message
  signal(leftOffice[doctorId])
}
```

### Receptionist code

```
run() {
  for each patient {
    wait(receptionistQueue)
    wait(mutex for receptionistQueue)
    patient = dequeue(receptionistQueue)
    signal(mutex for receptionistQueue)

    print register message for patient

    signal(checkin[patient])

    enqueue(patient) to nurseQueue
  }
}
```

### Nurse code

```
remainingMetrics = [remainingPatients, remainingNurses]

canIQuit() {
  canQuit = false
  wait(mutex for remainingMetrics)
  if remainingNurses > remainingPatients {
    remainingNurses--
    canQuit = true
  }
  return canQuit
}

run() {
  while !canIQuit() {
    wait(doctorReady[id])

    wait(nurseQueue)
    wait(mutex for nurseQueue)
    patient = dequeue(nurseQueue)
    signal(mutex for nurseQueue)

    wait(patientReady[id])

    print patient led to office message

    // This tells the doctor which patient they are waiting
    // for, and tells the patient which doctor they have.
    // These also don't need mutexes since assignedDoctor
    // is only written once per index, and the index
    // for assignedPatient is only written by this thread
    assignedDoctor[patient.id] = id
    assignedPatient[id] = patient.id

    signal(nurseProcessed[patient.id])

    wait(mutex for remainingMetrics)
    remainingPatients--
    signal(mutex for remainingMetrics)
  }
}
```

### Doctor Code

```
remainingMetrics = [remainingPatients, remainingNurses]

canIQuit() {
  canQuit = false
  wait(mutex for remainingMetrics)
  if remainingNurses > remainingPatients {
    remainingNurses--
    canQuit = true
  }
  return canQuit
}

run() {
  while !canIQuit() {
    signal(readyDoctors[id])
    wait(waitingForPatient[id])

    patient = assignedPatient[id]

    print patient symptoms message

    signal(doctorFinished[patient.id])
    wait(patientHasLeft[id])

    wait(mutex for remainingMetrics)
    remainingPatients--
    signal(mutex for remainingMetrics)
  }
}
```

### Main (Entry)

```text
main(args) {
  check if args >= 2

  doctor_count = args[0]
  patient_count = args[1]

  check if 0 < doctor_count <= 3
  check if 0 < patient_count <= 15

  state = create state object that holds state for all threads

  threads = thread array for all runnables
  add receptionist to thread array
  add doctors to thread array
  add patients to thread array

  for each thread in threads, start

  for each thread in threads, join
}
```

### Global State Object

```
init() {
  initialize all semaphores as stated at the top of the document

  assignedDoctor = int[patientCount]
  assignedPatient = int[doctorCount]

  receptionistQueue = new queue for patients
  nurseQueue = new queue for patients
}
```
