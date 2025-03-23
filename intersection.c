/*
 * Operating Systems (2INC0) Practical Assignment
 * Threading
 *
 * Intersection Part [REPLACE WITH PART NUMBER]
 * 
 * STUDENT_NAME_1 (STUDENT_NR_1)
 * STUDENT_NAME_2 (STUDENT_NR_2)
 * STUDENT_NAME_3 (STUDENT_NR_3)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#include "arrivals.h"
#include "intersection_time.h"
#include "input.h"

// TODO: Global variables: mutexes, data structures, etc...
pthread_mutex_t crossroad_lock;

int show_debug_traces = 0;

// a data structure to represent the arrival of a car
typedef struct
{
  char thread_name[10];
  Car_Arrival* cars_at_lane;
  sem_t* lane_semaphore;
} Lane_Info;

/* 
 * curr_car_arrivals[][][]
 *
 * A 3D array that stores the arrivals that have occurred
 * The first two indices determine the entry lane: first index is Side, second index is Direction
 * curr_arrivals[s][d] returns an array of all arrivals for the entry lane on side s for direction d,
 *   ordered in the same order as they arrived
 */
static Car_Arrival curr_car_arrivals[4][4][20];

/*
 * car_sem[][]
 *
 * A 2D array that defines a semaphore for each entry lane,
 *   which are used to signal the corresponding traffic light that a car has arrived
 * The two indices determine the entry lane: first index is Side, second index is Direction
 */
static sem_t car_sem[4][4];

/*
 * supply_cars()
 *
 * A function for supplying car arrivals to the intersection
 * This should be executed by a separate thread
 */
static void* supply_cars()
{
  if (show_debug_traces) 
  {
    printf("supply_cars: START\n");
  }
  int t = 0;
  int num_curr_arrivals[4][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};

  // for every arrival in the list
  for (int i = 0; i < sizeof(input_car_arrivals)/sizeof(Car_Arrival); i++)
  {
    // get the next arrival in the list
    Car_Arrival arrival = input_car_arrivals[i];
    // wait until this arrival is supposed to arrive
    sleep(arrival.time - t);
    t = arrival.time;
    // store the new arrival in curr_arrivals
    curr_car_arrivals[arrival.side][arrival.direction][num_curr_arrivals[arrival.side][arrival.direction]] = arrival;
    num_curr_arrivals[arrival.side][arrival.direction] += 1;

    if (show_debug_traces) 
    {
      printf(
        "car arriving side=%d direction=%d, car-id=%d\n", 
        arrival.side,
        arrival.direction,
        arrival.id
      );
    }

    // increment the semaphore for the traffic light that the arrival is for
    sem_post(&car_sem[arrival.side][arrival.direction]);

    if (show_debug_traces) 
    {
      int value; 
      sem_getvalue(&car_sem[arrival.side][arrival.direction], &value); 
      printf(
        "SEM(%p) value=%d (side=%d direction=%d, car-id=%d)\n", 
        &car_sem[arrival.side][arrival.direction],
        value,
        arrival.side,
        arrival.direction,
        arrival.id
      );
    }
  }

  if (show_debug_traces) 
  {
    printf("supply_cars: END\n");
  }
  return(0);
}


/*
 * manage_light(void* arg)
 *
 * A function that implements the behaviour of a traffic light
 */
static void* manage_light(void* arg)
{
  Lane_Info* lane = (Lane_Info*)arg;
  if (show_debug_traces) 
  {
    printf("\n Thread started: %s (%p)\n", lane->thread_name, lane->lane_semaphore);
  }

  int current_car = 0;

  // calculate the timeout for the end of the program
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += (END_TIME - get_time_passed());

  while (true) {
    int semaphore_wait_result = sem_timedwait(lane->lane_semaphore, &ts);
    if (semaphore_wait_result == -1) {
      if (errno == ETIMEDOUT) {
        if (show_debug_traces) 
        {
          printf("%s: sem_timedwait() timed out\n", lane->thread_name);
        }
        return 0;
      }
    }

    Car_Arrival* car_at_crossing = lane->cars_at_lane + current_car;

    if (show_debug_traces) 
    {
      printf(
        "%s: car ARRIVES at %d %d turns green at time %d for car %d\n",
        lane->thread_name,
        car_at_crossing->side,
        car_at_crossing->direction,
        get_time_passed(),
        car_at_crossing->id
      );
    }

    pthread_mutex_lock(&crossroad_lock);
    printf(
      "traffic light %d %d turns green at time %d for car %d\n",
      car_at_crossing->side,
      car_at_crossing->direction,
      get_time_passed(),
      car_at_crossing->id
    );

    sleep(CROSS_TIME);

    printf(
      "traffic light %d %d turns red at time %d\n",
      car_at_crossing->side,
      car_at_crossing->direction,
      get_time_passed()
    );
    pthread_mutex_unlock(&crossroad_lock);
    current_car++;
  }

  // TODO:
  // while not all arrivals have been handled, repeatedly:
  //  - wait for an arrival using the semaphore for this traffic light
  //  - lock the right mutex(es)
  //  - make the traffic light turn green
  //  - sleep for CROSS_TIME seconds
  //  - make the traffic light turn red
  //  - unlock the right mutex(es)

  return(0);
}


int main(int argc, char * argv[])
{
  printf("\n Start program\n");
  pthread_t traffic_light_threads[4][4];
  Lane_Info traffic_light_infos[4][4];

  if (pthread_mutex_init(&crossroad_lock, NULL) != 0) { 
    printf("\n Initilizing crossroad_lock failed\n"); 
    return -1;
  } 

  // create semaphores to wait/signal for arrivals
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      sem_init(&car_sem[i][j], 0, 0);
    }
  }

  // start the timer
  start_time();

  // TODO: create a thread per traffic light that executes manage_light
  // Create for each traffic light a thread
  // create semaphores to wait/signal for arrivals
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      if (show_debug_traces) 
      {
        sprintf(traffic_light_infos[i][j].thread_name, "Thread-[%d][%d]", i, j);
      }
      traffic_light_infos[i][j].lane_semaphore = &car_sem[i][j];
      traffic_light_infos[i][j].cars_at_lane = &curr_car_arrivals[i][j];
      int creation_result = pthread_create(
        &traffic_light_threads[i][j], 
        NULL, 
        manage_light, 
        &traffic_light_infos[i][j]
      );

      if (creation_result != 0) 
      {
        printf("traffic_light [%d,%d] thread creation failed\n", i, j);
      }
      
      if (show_debug_traces) 
      {
          printf(
            "Thread started: %s, sem_address=%p lane_sem_address=%p\n", 
            traffic_light_infos[i][j].thread_name, 
            &car_sem[i][j], 
            traffic_light_infos[i][j].lane_semaphore
          );      
        }
      }
  }

  // TODO: create a thread that executes supply_cars()
  pthread_t thread_supply_car;
  if (pthread_create(&thread_supply_car, NULL, supply_cars, NULL) != 0) 
  {
    printf("thread_supply_car thread creation failed\n");
  };


  // TODO: wait for all threads to finish
  // destroy semaphores
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      pthread_join(traffic_light_threads[i][j], NULL);
      if (show_debug_traces) 
      {
          printf("thread %d %d stopped\n", i, j);
      }
      sem_destroy(&car_sem[i][j]);
    }
  }

  pthread_mutex_destroy(&crossroad_lock);
}
