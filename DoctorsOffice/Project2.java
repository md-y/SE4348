import java.util.LinkedList;
import java.util.concurrent.Semaphore;

public class Project2 {
  public static void main(String[] args) {
    if (args.length < 2) {
      System.out.println("Usage: Project2 <doctor count> <patient count>");
      return;
    }

    int doctorCount = Integer.parseInt(args[0]);
    int patientCount = Integer.parseInt(args[1]);

    if (doctorCount > 3 || doctorCount < 1) {
      System.out.println("Doctor count is invalid. Max of 3, min of 1");
      return;
    }

    if (patientCount > 15 || patientCount < 1) {
      System.out.println("Patient count is invalid. Max of 3, min of 1");
      return;
    }

    System.out.println(
        "Run with " + patientCount + " patients, " + doctorCount + " nurses, " + doctorCount + " doctors");

    OfficeContext context = new OfficeContext(doctorCount, patientCount);

    // Create threads
    Thread threads[] = new Thread[doctorCount * 2 + patientCount + 1];
    int i = 0;
    threads[i++] = new Thread(new Receptionist(0, context));

    for (int j = 0; j < doctorCount; j++) {
      threads[i++] = new Thread(new Doctor(j, context));
      threads[i++] = new Thread(new Nurse(j, context));
    }

    for (int j = 0; j < patientCount; j++) {
      threads[i++] = new Thread(new Patient(j, context));
    }

    // Start threads
    for (Thread t : threads) {
      t.start();
    }

    // Wait for all threads to finish
    for (Thread t : threads) {
      try {
        t.join();
      } catch (InterruptedException e) {
        e.printStackTrace();
      }
    }

    System.out.println("Simulation complete");
  }
}

/**
 * Used to store the state of the office and provide synchronization for the
 * threads.
 */
class OfficeContext {
  public OfficeQueue<Patient> receptionistQueue;
  public OfficeQueue<Patient> nurseQueue;

  public Semaphore patientsReadyForNurse[];

  // For nurse to doctor hand-off:
  public Semaphore doctorsWaitingForPatient[];
  public Semaphore readyDoctors[];
  public Semaphore doctorFinished[];
  public Semaphore patientHasLeft[];
  public int assignedDoctor[];
  public int assignedPatient[];

  public RemainingItemsTracker nurseTracker;
  public RemainingItemsTracker doctorTracker;

  public final int totalPatients;

  OfficeContext(int doctorCount, int patientCount) {
    totalPatients = patientCount;

    receptionistQueue = new OfficeQueue<Patient>(patientCount);
    nurseQueue = new OfficeQueue<Patient>(patientCount);

    assignedDoctor = new int[patientCount];
    patientsReadyForNurse = initSemaphores(patientCount, 0);
    patientHasLeft = initSemaphores(patientCount, 0);

    readyDoctors = initSemaphores(doctorCount, 0);
    doctorsWaitingForPatient = initSemaphores(doctorCount, 0);
    doctorFinished = initSemaphores(doctorCount, 0);
    assignedPatient = new int[doctorCount];

    nurseTracker = new RemainingItemsTracker(patientCount, doctorCount);
    doctorTracker = new RemainingItemsTracker(patientCount, doctorCount);
  }

  private Semaphore[] initSemaphores(int size, int initialValue) {
    Semaphore semaphores[] = new Semaphore[size];
    for (int i = 0; i < size; i++) {
      semaphores[i] = new Semaphore(initialValue, true);
    }
    return semaphores;
  }

  /**
   * Acquire a semaphore, and if there is an exception, quit.
   */
  public static void safeAcquire(Semaphore s) {
    try {
      s.acquire();
    } catch (InterruptedException e) {
      e.printStackTrace();
      System.exit(1);
    }
  }
}

/**
 * Generic office person with a reference to an office context and ID.
 */
class OfficePerson {
  protected OfficeContext context;
  protected int id;

  OfficePerson(int identifier, OfficeContext semaphoreHandler) {
    context = semaphoreHandler;
    id = identifier;
  }
}

/**
 * A semaphore-based mutex class for protecting a value.
 */
class MutexValue<V> {
  public V value;
  private Semaphore mutex = new Semaphore(1, true);

  MutexValue(V initialValue) {
    value = initialValue;
  }

  public void acquire() {
    OfficeContext.safeAcquire(mutex);
  }

  public void release() {
    mutex.release();
  }
}

/**
 * Used to check if a worker can stop working based on the number of items left
 * and workers still running.
 */
class RemainingItemsTracker {
  // Array is a tuple: [items left, workers left]
  private MutexValue<Integer[]> remaining;

  RemainingItemsTracker(int totalItems, int totalWorkers) {
    remaining = new MutexValue<Integer[]>(new Integer[] { totalItems, totalWorkers });
  }

  /**
   * Are there few enough remaining items so that this worker can quit?
   * If true, the worker is considered done. This also assumes all workers are
   * equal and always trying to process.
   */
  public boolean attemptToQuit() {
    boolean shouldQuit = false;
    remaining.acquire();
    if (remaining.value[1] > remaining.value[0]) {
      shouldQuit = true;
      remaining.value[1]--;
    }
    remaining.release();
    return shouldQuit;
  }

  public void decrementRemaining() {
    remaining.acquire();
    remaining.value[0]--;
    remaining.release();
  }
}

/**
 * A queue that can be safely added to by a producer and detect when its item
 * was processed.
 */
class OfficeQueue<Person extends OfficePerson> {
  private MutexValue<LinkedList<Person>> queue = new MutexValue<>(new LinkedList<>());

  // Number of unprocessed items in the queue
  private Semaphore waitingSize = new Semaphore(0, true);

  // Semaphores for each producer to wait for their item to be processed
  private Semaphore waitingProducers[];

  OfficeQueue(int maxProducers) {
    waitingProducers = new Semaphore[maxProducers];
    for (int i = 0; i < maxProducers; i++) {
      waitingProducers[i] = new Semaphore(0, true);
    }
  }

  /**
   * Safely add a person to the queue
   */
  public void enqueue(Person p) {
    queue.acquire();
    queue.value.add(p);
    queue.release();
    waitingSize.release();
  }

  /**
   * Wait for an enqueue and dequeue the next person
   */
  public Person waitAndDequeue() {
    OfficeContext.safeAcquire(waitingSize);
    queue.acquire();
    Person p = queue.value.poll();
    queue.release();
    return p;
  }

  /**
   * Wait for a signal that a person has been processed via their ID
   */
  public void waitForProcessing(Person p) {
    OfficeContext.safeAcquire(waitingProducers[p.id]);
  }

  /**
   * Signal that a person has been processed by their id
   */
  public void signalProcessed(Person p) {
    waitingProducers[p.id].release();
  }
}

class Receptionist extends OfficePerson implements Runnable {
  Receptionist(int id, OfficeContext handler) {
    super(id, handler);
  }

  public void run() {
    // The receptionist will see every patient, so it can be a simple for loop
    for (int i = 0; i < context.totalPatients; i++) {
      Patient p = context.receptionistQueue.waitAndDequeue();
      System.out.println("Receptionist registers patient " + p.id);
      context.receptionistQueue.signalProcessed(p);

      // Tell nurse that a patient will be ready soon
      context.nurseQueue.enqueue(p);
    }
  }
}

class Doctor extends OfficePerson implements Runnable {
  Doctor(int id, OfficeContext handler) {
    super(id, handler);
  }

  public void run() {
    while (!context.doctorTracker.attemptToQuit()) {
      context.readyDoctors[id].release(); // Signal to nurse that doctor is ready
      OfficeContext.safeAcquire(context.doctorsWaitingForPatient[id]); // Wait for patient
      int assignedPatient = context.assignedPatient[id];
      System.out.println("Doctor " + id + " listens to symptoms from patient " + assignedPatient);
      context.doctorFinished[id].release(); // Signal to patient that doctor is done
      OfficeContext.safeAcquire(context.patientHasLeft[assignedPatient]); // Wait for patient to leave
      context.doctorTracker.decrementRemaining();
    }
  }
}

class Nurse extends OfficePerson implements Runnable {
  Nurse(int id, OfficeContext handler) {
    super(id, handler);
  }

  public void run() {
    while (!context.nurseTracker.attemptToQuit()) {
      OfficeContext.safeAcquire(context.readyDoctors[id]); // Wait for doctor to be ready
      Patient p = context.nurseQueue.waitAndDequeue();
      OfficeContext.safeAcquire(context.patientsReadyForNurse[p.id]); // Wait for patient to be ready
      System.out.println("Nurse " + id + " takes patient " + p.id + " to doctor's office");
      context.assignedDoctor[p.id] = id; // Safe because each index is only written to once
      context.assignedPatient[id] = p.id; // Safe because this is the only thread for this index
      context.nurseQueue.signalProcessed(p);
      context.nurseTracker.decrementRemaining();
    }
  }
}

class Patient extends OfficePerson implements Runnable {
  Patient(int id, OfficeContext handler) {
    super(id, handler);
  }

  public void run() {
    // Receptionist check-in
    System.out.println("Patient " + id + " enters waiting room, waits for receptionist");
    context.receptionistQueue.enqueue(this);
    context.receptionistQueue.waitForProcessing(this);
    System.out.println("Patient " + id + " leaves receptionist and sits in waiting room");

    // Patient is led by a nurse
    context.patientsReadyForNurse[id].release();
    context.nurseQueue.waitForProcessing(this);
    int assignedDoctor = context.assignedDoctor[id];
    System.out.println("Patient " + id + " enters doctor " + assignedDoctor + "'s office");

    // Patient meets with doctor
    context.doctorsWaitingForPatient[assignedDoctor].release();
    OfficeContext.safeAcquire(context.doctorFinished[assignedDoctor]);
    System.out.println("Patient " + id + " receives advice from doctor " + assignedDoctor);

    System.out.println("Patient " + id + " leaves");
    context.patientHasLeft[id].release();
  }
}
