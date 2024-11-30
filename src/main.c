#include <pthread.h> // for pthread_create, pthread_join, pthread_t
#include <stdio.h>   // for printf, fprintf, NULL, stderr

int counter = 0;

void *
increment (void *arg)
{
  int id = *((int *)arg);
  for (int i = 0; i < 100000; ++i)
    {
      counter++;
      if (i % 10000 == 0)
        {
          printf ("Thread %d at iteration %d\n", id, i);
        }
    }
  printf ("Thread %d finished\n", id);
  return NULL;
}

int
main ()
{
  pthread_t t1;
  pthread_t t2;
  int id1 = 1;
  int id2 = 2;

  printf ("Starting threads...\n");

  if (pthread_create (&t1, NULL, increment, &id1))
    {
      fprintf (stderr, "Error creating thread 1\n");
      return 1;
    }
  if (pthread_create (&t2, NULL, increment, &id2))
    {
      fprintf (stderr, "Error creating thread 2\n");
      return 1;
    }

  printf ("Waiting for thread 1 to join...\n");
  if (pthread_join (t1, NULL))
    {
      fprintf (stderr, "Error joining thread 1\n");
      return 2;
    }
  printf ("Thread 1 joined\n");

  printf ("Waiting for thread 2 to join...\n");
  if (pthread_join (t2, NULL))
    {
      fprintf (stderr, "Error joining thread 2\n");
      return 2;
    }
  printf ("Thread 2 joined\n");

  printf ("Final counter value: %d\n", counter);
  return 0;
}
