import java.util.ArrayDeque;
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

class OfficeContext {
  public OfficeQueue<Patient> receptionistQueue;
  public OfficeQueue<Patient> nurseQueue;
  public OfficeQueue<Patient> doctorQueue;

  public Semaphore readyDoctors[];
  public int assignedDoctor[];

  public final int totalPatients;

  OfficeContext(int doctorCount, int patientCount) {
    receptionistQueue = new OfficeQueue<Patient>(patientCount, 1);
    nurseQueue = new OfficeQueue<Patient>(patientCount, doctorCount);
    doctorQueue = new OfficeQueue<Patient>(patientCount, doctorCount);

    totalPatients = patientCount;
    assignedDoctor = new int[patientCount];
    for (int i = 0; i < patientCount; i++) {
      assignedDoctor[i] = -1;
    }

    readyDoctors = new Semaphore[doctorCount];
    for (int i = 0; i < doctorCount; i++) {
      readyDoctors[i] = new Semaphore(0, true);
    }
  }

  public static void safeAquire(Semaphore s) {
    try {
      s.acquire();
    } catch (InterruptedException e) {
      e.printStackTrace();
      System.exit(1);
    }
  }
}

class OfficePerson {
  protected OfficeContext context;
  protected int id;

  OfficePerson(int identifier, OfficeContext semaphoreHandler) {
    context = semaphoreHandler;
    id = identifier;
  }
}

class OfficeQueue<Person extends OfficePerson> {
  private ArrayDeque<Person> queue;
  private Semaphore queueMutex = new Semaphore(1, true);
  private Semaphore waitingSize = new Semaphore(0, true);
  private Semaphore waitingProducers[];

  private Semaphore processingItemsMutex = new Semaphore(1, true); // Mutex is for both
  private int processingConsumers;
  private int remainingItems;

  OfficeQueue(int producerSize, int consumerSize) {
    processingConsumers = consumerSize;
    remainingItems = producerSize;
    queue = new ArrayDeque<Person>(producerSize);
    waitingProducers = new Semaphore[producerSize];
    for (int i = 0; i < producerSize; i++) {
      waitingProducers[i] = new Semaphore(0, true);
    }
  }

  public void enqueue(Person p) {
    OfficeContext.safeAquire(queueMutex);
    queue.add(p);
    queueMutex.release();
    waitingSize.release();
  }

  public Person waitAndDequeue() {
    OfficeContext.safeAquire(waitingSize);
    OfficeContext.safeAquire(queueMutex);
    Person p = queue.poll();
    queueMutex.release();
    return p;
  }

  public void waitForProcessing(Person p) {
    OfficeContext.safeAquire(waitingProducers[p.id]);
  }

  public void signalProcessed(Person p) {
    OfficeContext.safeAquire(processingItemsMutex);
    remainingItems--;
    processingItemsMutex.release();
    waitingProducers[p.id].release();
  }

  public boolean canConsumerQuit() {
    boolean canQuit = false;
    OfficeContext.safeAquire(processingItemsMutex);
    if (processingConsumers > remainingItems) {
      canQuit = true;
      processingConsumers--;
    }
    processingItemsMutex.release();
    return canQuit;
  }
}

class Receptionist extends OfficePerson implements Runnable {
  Receptionist(int id, OfficeContext handler) {
    super(id, handler);
  }

  public void run() {
    while (!context.receptionistQueue.canConsumerQuit()) {
      Patient p = context.receptionistQueue.waitAndDequeue();
      System.out.println("Receptionist registers patient " + p.id);
      context.receptionistQueue.signalProcessed(p);
    }
  }
}

class Doctor extends OfficePerson implements Runnable {
  Doctor(int id, OfficeContext handler) {
    super(id, handler);
  }

  public void run() {
    while (!context.doctorQueue.canConsumerQuit()) {
      context.readyDoctors[id].release();
      Patient p = context.doctorQueue.waitAndDequeue();
      System.out.println("Doctor " + id + " listens to symptoms from patient " + p.id);
      context.doctorQueue.signalProcessed(p);
    }
  }
}

class Nurse extends OfficePerson implements Runnable {
  Nurse(int id, OfficeContext handler) {
    super(id, handler);
  }

  public void run() {
    while (!context.nurseQueue.canConsumerQuit()) {
      OfficeContext.safeAquire(context.readyDoctors[id]);
      Patient p = context.nurseQueue.waitAndDequeue();
      System.out.println("Nurse " + id + " takes patient " + p.id + " to doctor's office");
      context.assignedDoctor[p.id] = id; // Safe because each index is only written to once
      context.nurseQueue.signalProcessed(p);
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
    context.nurseQueue.enqueue(this);
    context.nurseQueue.waitForProcessing(this);
    int assignedDoctor = context.assignedDoctor[id];
    System.out.println("Patient " + id + " enters doctor " + assignedDoctor + "'s office");

    // Patient meets with doctor
    context.doctorQueue.enqueue(this);
    context.doctorQueue.waitForProcessing(this);
    System.out.println("Patient " + id + " receives advice from doctor " + assignedDoctor);

    System.out.println("Patient " + id + " leaves");
  }
}
