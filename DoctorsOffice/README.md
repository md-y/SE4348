## Project 2

Project summary is [here](./summary.md). Design document is [here](./design.md)

To compile and run on **cs1.utdallas.edu**:

```bash
export JAVA_HOME=/usr/local/jdk-21.0.2
export PATH=$JAVA_HOME/bin:$PATH
/usr/local/jdk-21.0.2/bin/javac Project2.java
/usr/local/jdk-21.0.2/bin/java Project2 3 15
```

Specific Java requirements:

- You must use Java Threads and Java Semaphores (`java.util.concurrent.Semaphore`)
- You should not use the “synchronized” keyword
- You should not use any Java classes that have built-in mutual exclusion
- Any mechanisms for thread coordination other than the semaphore are not allowed
