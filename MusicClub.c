#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <readline/readline.h>  //for {readline}
#include <readline/history.h>  //for {readline}

/*-----------------------Function Decleration-------------------*/
int init_participant(int id,char* name,char instrument, int arrival_time);
int init_acoustic_stages(int id);
int init_electric_stages(int id);
void *participant_thread_function(void *data);
void *find_stage_function(void *data);
char ** tokenizer(char *input_line, char *delimiter) ;





typedef struct participant
{
    int id;
    char *name;
    char instrument;
    int arrival_time;
    int stage_type; //-1 DEFAULT | 0 Acoustic | 1 Electric |
    int singer_mode;  //0 DEFAULT | 1 Solo | 2 Duo |
    int performance_state; //0 DEFAULT | -1 Left without performing | 1 Performing | 2 Performance Done |
    int stage;

    pthread_mutex_t participant_lock;   //Mutex lock to acess participant info
    pthread_t seeker;   //Thread to search stage for this participant
    sem_t sem_par;  //Semaphore to timed wait on before leaving

}participant;



typedef struct stage
{
    int id;
    int participant_id; //-1 DEFAULT or Empty | id of performing Participant (Will be singer Id whenthe singer comes in as a solo performer)
    int singer_id;  //-1 DEFAULT or Empty | id of performing Singer (Only when the singer comes in as a duo performer)
    sem_t sem_par_space;    // Semaphore for Participant Space on Stage
    sem_t sem_sing_space;   // Semaphore for Singer Space on Stage

    pthread_mutex_t stage_lock;

}stage;



//----------Global Variables--------------------


participant *list_of_participants[200];
stage *list_of_acoustic_stages[200];
stage *list_of_electric_stages[200];

sem_t sem_club_coordinators;


int num_participants;
int num_acoustic_stages;
int num_electric_stages;
int num_coordinators;
int lower_time_limit;
int upper_time_limit;
int patience;

pthread_t singer_collector[100];
int singer_tshirt = 0;






int main()
{
    srand(time(0)); //to seed rand() function with current time (Not necessary just done to get rand() to vary more)


    printf("Enter Number of participants, Number of Acoustic Stages, Number of Electric Stages, Number of Club Coordinators,\nLower Limit of Performance Time, Upper Limit of Performance Time, And Patienceof Participants\n(Seperated by \" \")\n");

    scanf("%d %d %d %d %d %d %d",&num_participants,&num_acoustic_stages,&num_electric_stages,&num_coordinators,&lower_time_limit,&upper_time_limit,&patience);
    printf("Enter <Name> <Instrument Initial> <Arrival Time>\n");
    for(int i = 0; i < num_participants;)
    {
        char *input = readline("Participant : ");
        char **inputlist = tokenizer(input," ");
        int inputcount = 0;
        while(inputlist[inputcount] != NULL)inputcount++;
        if(inputcount != 3)
        {
            printf("There was an error in the input please try again : \n");
        }
        else
        {
            init_participant(i,inputlist[0],inputlist[1][0],atoi(inputlist[2]));
            i++;
        }
    }

    sem_init(&sem_club_coordinators, 0, num_coordinators);  //Initiating number of co-ordinators 

    for(int i = 0; i < num_acoustic_stages ; i++)
    {
        init_acoustic_stages(i);
    }

    for (int i = 0; i < num_electric_stages ; i++)
    {
        init_electric_stages(i);
    }



    //---------------Spawning Participant threads & Joining them-----------------


    pthread_t *participant_threads = (pthread_t*) malloc(sizeof(pthread_t)*num_participants);

    //Thread Spawning 
    for (int i = 0; i < num_participants ; i++)
    pthread_create(&participant_threads[i], NULL, &participant_thread_function, (void*)(list_of_participants[i])); 

    //Thread Joining
    for (int i = 0; i < num_participants ; i++)
    pthread_join(participant_threads[i],NULL); 

    printf("\033[0;31m> Finished \n\033[0m");
    return 0;
}


//-------------------------Thread Functions----------------------------------------


void *participant_thread_function(void *data)
{
    participant *par = (participant *)data;
    sleep(par->arrival_time);
    printf("\033[1;32m> %s %c arrived\n\033[0m", par->name, par->instrument);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += patience;
    
    pthread_create(&(par->seeker), NULL, &find_stage_function, (void*)(par));
    int ret = sem_timedwait(&(par->sem_par),&ts);
    //sem_post(&(par->sem_par));  ->Ye use karna hai jbb bhi stage mill jaye tbb
    if (ret == -1 && errno == ETIMEDOUT)
    {
        pthread_mutex_lock(&(par->participant_lock));
        if(par->performance_state == 0)
        {
            par->performance_state = -1;
            printf("\033[1;31m> %s left because of impatience  \n\033[0m", par->name);
        }
        pthread_mutex_unlock(&(par->participant_lock));
        pthread_exit(NULL);
        
        //perform nahi krr raha so lock acquire krr if performance_state = 0 hai toh performance_state = -1 krr ke kick krr de
        //Edge case performance_state = 1 hai toh phir just timeout hone se pehle usko stage mila thaa so let him perform (We might never come across this state)
    }
    else if (ret == 0)
    {
        pthread_mutex_lock(&par->participant_lock);
        if(par->performance_state ==1 )
        {
            int ptime = lower_time_limit + (rand() % (upper_time_limit - lower_time_limit));
            if(par->stage_type == 0 && par->singer_mode != 2)
            {
                //acoustic 
                int stagenum = par->stage;  
                int joined = -1;
                int tshirt;
                printf("\033[0;33m> %s performing %c at acoustic stage (id: %d) for %d sec\n\033[0m", par->name, par->instrument, par->stage, ptime);
                pthread_mutex_unlock(&par->participant_lock);
                sleep(ptime);
                //stage lock acquire krr aur check krr if any singer came to duo in meantime when we were sleeping
                pthread_mutex_lock(&(list_of_acoustic_stages[stagenum]->stage_lock));
                if(list_of_acoustic_stages[stagenum]->singer_id != -1)
                {
                    joined = list_of_acoustic_stages[stagenum]->singer_id;
                    list_of_acoustic_stages[stagenum]->singer_id = -1;
                }
                pthread_mutex_unlock(&(list_of_acoustic_stages[stagenum]->stage_lock));
                if(joined != -1)
                {
                    sleep(2);
                    //stage ka singer semaphore increase krr de
                    sem_post(&(list_of_acoustic_stages[stagenum]->sem_sing_space));
                    printf("\033[1;35m> %s collecting t-shirt\n\033[0m", list_of_participants[joined]->name);
                    tshirt = sem_wait(&sem_club_coordinators);
                    if (tshirt == 0)
                    {
                        sleep(2);
                        //print that he recieved T-shirt with id of performer = joined
                        printf("\033[1;36m> %s left after collecting t-shirt\n\033[0m", list_of_participants[joined]->name);

                    }
                }
                //Also Duo ka time extension krr le
                //stage ka participant semaphore increase karna hai
                sem_post(&(list_of_acoustic_stages[stagenum]->sem_par_space));
                //print that the performance is over
                printf("\033[0;34m> %s performance at acoustic stage (id: %d) ended\n\033[0m", par->name, par->stage);
                printf("\033[1;35m> %s collecting t-shirt\n\033[0m", par->name);
                tshirt = sem_wait(&sem_club_coordinators);
                if (tshirt == 0)
                {
                    sleep(2);
                    //print that he recieved T-shirt
                    printf("\033[1;36m> %s left after collecting t-shirt\n\033[0m", par->name);
                    sem_post(&sem_club_coordinators);

                }
            }
            else if (par->stage_type == 1 && par->singer_mode != 2)
            {
                //electric
                int stagenum = par->stage;
                int joined = -1;
                int tshirt;
                printf("\033[0;33m> %s performing %c at electric stage (id: %d) for %d sec\n\033[0m", par->name, par->instrument, par->stage, ptime);
                pthread_mutex_unlock(&par->participant_lock);
                sleep(ptime);
                //stage lock acquire krr aur check krr if any singer came to duo in meantime when we were sleeping
                pthread_mutex_lock(&(list_of_electric_stages[stagenum]->stage_lock));
                if(list_of_electric_stages[stagenum]->singer_id != -1)
                {
                    joined = list_of_electric_stages[stagenum]->singer_id;
                    list_of_electric_stages[stagenum]->singer_id = -1;
                }
                pthread_mutex_unlock(&(list_of_electric_stages[stagenum]->stage_lock));
                if(joined != -1)
                {
                    sleep(2);
                    //stage ka singer semaphore increase karna hai
                    sem_post(&(list_of_electric_stages[stagenum]->sem_sing_space));
                    printf("\033[1;35m> %s collecting t-shirt\n\033[0m", list_of_participants[joined]->name);
                    tshirt = sem_wait(&sem_club_coordinators);
                    if (tshirt == 0)
                    {
                        sleep(2);
                        //print that he recieved T-shirt with id of performer = joined
                        printf("\033[1;36m> %s left after collecting t-shirt\n\033[0m", list_of_participants[joined]->name);

                    }
                }
                //Also Duo ka time extension krr le
                //stage ka participant semaphore increase karna hai 
                sem_post(&(list_of_electric_stages[stagenum]->sem_par_space));
                //print that the performance is over 
                printf("\033[0;34m> %s performance at electric stage (id: %d) ended\n\033[0m", par->name, par->stage);
                printf("\033[1;35m> %s collecting t-shirt\n\033[0m", par->name);
                tshirt = sem_wait(&sem_club_coordinators);
                if (tshirt == 0)
                {
                    sleep(2);
                    //print that he recieved T-shirt
                    printf("\033[1;36m> %s left after collecting t-shirt\n\033[0m", par->name);
                    sem_post(&sem_club_coordinators);
                }
            }
            else if (par->singer_mode == 2)
            {
                //Duo announcement

                if(par->stage_type == 0)
                {
                    //acoustic Duo
                    
                    printf("\033[0;36m> %s joined  performance on acoustic stage (id = %d), performance extended by 2 secs\n\033[0m", par->name, par->stage);
                    
                }

                else if(par->stage_type == 1)
                {
                    //Electric Duo
                    
                    printf("\033[0;36m> %s joined joined  performance on electric stage (id = %d), performance extended by 2 secs\n\033[0m", par->name, par->stage);
                
                }
                pthread_mutex_unlock(&par->participant_lock);
            }
            else
            {
                pthread_mutex_unlock(&par->participant_lock);   //covering all situations for lock,unlock pairs
            }
        }
        else
        {
            pthread_mutex_unlock(&par->participant_lock);   //No matter what happens we always bring lock and unlock in pairs 
        }
        
    }
}




void *find_stage_function(void *data)
{
    participant *par = (participant *)data;



    if(par->instrument == 'p' || par->instrument == 'g')
    {
        //as it is piano or guitar we search both acoustic and electric stages
        int as = 1;
        int es = 1;
        while(as && es)
        {
            for (int i = 0; i < num_acoustic_stages ; i++)
            {
                as = sem_trywait(&(list_of_acoustic_stages[i]->sem_par_space));
                if (as == 0)
                {
                    //Acoustic stage found
                    //acquire par lock
                    pthread_mutex_lock(&(par->participant_lock));
                    //if par has left sem_post immediately
                    if(par->performance_state == -1) 
                    {
                        sem_post(&(list_of_acoustic_stages[i]->sem_par_space));
                        //this means we didnt use the stage at all and exited
                        pthread_mutex_unlock(&(par->participant_lock));
                        //release par lock
                        pthread_exit(NULL);
                    }
                    else
                    {
                        //dont sempost (sempost in participant thread function after performance)
                        //update par variables 
                        par->stage = i;
                        par->performance_state = 1;
                        par->stage_type = 0;
                        pthread_mutex_unlock(&(par->participant_lock));
                        //release par lock
                        sem_post(&(par->sem_par)); 
                        pthread_exit(NULL);
                    }
                }
            }

            //couldn't acquire any of the acoustic stages so try electric stages
            for (int j = 0; j < num_electric_stages ; j++)
            {
                es = sem_trywait(&(list_of_electric_stages[j]->sem_par_space));
                if (es == 0)
                {
                    //Electric stage found
                    //acquire par lock
                    pthread_mutex_lock(&(par->participant_lock));
                    //if par has left sem_post immediately
                    if(par->performance_state == -1) 
                    {
                        sem_post(&(list_of_electric_stages[j]->sem_par_space));
                        //this means we didnt use the stage at all and exited
                        pthread_mutex_unlock(&(par->participant_lock));
                        //release par lock
                        pthread_exit(NULL);
                    }
                    else
                    {
                        //dont sempost (sempost in participant thread function after performance)
                        //update par variables 
                        par->stage = j;
                        par->performance_state = 1;
                        par->stage_type = 1;
                        pthread_mutex_unlock(&(par->participant_lock));
                        //release par lock
                        sem_post(&(par->sem_par)); 
                        pthread_exit(NULL);
                    }
                }
            }

            //couldnt acquire any electric stages too so now go through all of them again
        }
    }





    else if (par->instrument == 'v')
    {
        //as it  is violin we only search acoustic stages
        int as = 1;

        while(as)
        {
            for (int i = 0; i < num_acoustic_stages ; i++)
            {
                as = sem_trywait(&(list_of_acoustic_stages[i]->sem_par_space));
                if (as == 0)
                {
                    //Acoustic stage found
                    //acquire par lock
                    pthread_mutex_lock(&(par->participant_lock));
                    //if par has left sem_post immediately
                    if(par->performance_state == -1) 
                    {
                        sem_post(&(list_of_acoustic_stages[i]->sem_par_space));
                        //this means we didnt use the stage at all and exited
                        pthread_mutex_unlock(&(par->participant_lock));
                        //release par lock
                        pthread_exit(NULL);
                    }
                    else
                    {
                        //dont sempost (sempost in participant thread function after performance)
                        //update par variables 
                        par->stage = i;
                        par->performance_state = 1;
                        par->stage_type = 0;
                        pthread_mutex_unlock(&(par->participant_lock));
                        //release par lock
                        sem_post(&(par->sem_par)); 
                        pthread_exit(NULL);
                    }
                }
            }
        }
    }




    else if (par->instrument == 'b')
    {
        //as it is  bass we only search electric stages
        int es = 1;
        while(es)
        {
            for (int j = 0; j < num_electric_stages ; j++)
            {
                es = sem_trywait(&(list_of_electric_stages[j]->sem_par_space));
                if (es == 0)
                {
                    //Electric stage found
                    //acquire par lock
                    pthread_mutex_lock(&(par->participant_lock));
                    //if par has left sem_post immediately
                    if(par->performance_state == -1) 
                    {
                        sem_post(&(list_of_electric_stages[j]->sem_par_space));
                        //this means we didnt use the stage at all and exited
                        pthread_mutex_unlock(&(par->participant_lock));
                        //release par lock
                        pthread_exit(NULL);
                    }
                    else
                    {
                        //dont sempost (sempost in participant thread function after performance)
                        //update par variables 
                        par->stage = j;
                        par->performance_state = 1;
                        par->stage_type = 1;
                        pthread_mutex_unlock(&(par->participant_lock));
                        //release par lock
                        sem_post(&(par->sem_par)); 
                        pthread_exit(NULL);
                    }
                }
            }

            //couldnt acquire any electric stages so now go through all of them again
        }
    }




    else if (par->instrument == 's')
    {
        //as it is a singer we search both acoustic and electric stages
        //however if we fail to find any stage as participannt(lead performer)
        //we immediately try to grab the same stage as duo performer

        
        int as = 1;
        int es = 1;
        int esmu = 1;
        int asmu = 1;
        while(as && es )
        {
            for (int i = 0; i < num_acoustic_stages ; i++)
            {
                as = sem_trywait(&(list_of_acoustic_stages[i]->sem_par_space));
                if (as == 0)
                {
                    //Acoustic stage found
                    //acquire par lock
                    pthread_mutex_lock(&(par->participant_lock));
                    //if par has left sem_post immediately
                    if(par->performance_state == -1) 
                    {
                        sem_post(&(list_of_acoustic_stages[i]->sem_par_space));
                        //this means we didnt use the stage at all and exited
                        pthread_mutex_unlock(&(par->participant_lock));
                        //release par lock
                        pthread_exit(NULL);
                    }
                    else
                    {
                        //dont sempost (sempost in participant thread function after performance)
                        //update par variables 
                        par->stage = i;
                        par->performance_state = 1;
                        par->stage_type = 0;
                        pthread_mutex_unlock(&(par->participant_lock));
                        //release par lock
                        sem_post(&(par->sem_par)); 
                        pthread_exit(NULL);
                    }
                }
                else if (as == -1)
                {
                    //Acoustic stage not found as a solo performer
                    asmu = sem_trywait(&(list_of_acoustic_stages[i]->sem_sing_space));
                    if (asmu == 0)
                    {
                        //Acoustic stage found as a duo performer
                        //acquire par lock
                        pthread_mutex_lock(&(par->participant_lock));
                        //if par has left sem_post immediately
                        if(par->performance_state == -1) 
                        {
                            sem_post(&(list_of_acoustic_stages[i]->sem_sing_space));
                            //this means we didnt use the stage at all and exited
                            pthread_mutex_unlock(&(par->participant_lock));
                            //release par lock
                            pthread_exit(NULL);
                        }
                        else
                        {
                            //dont sempost (sempost in participant thread function after performance)
                            //update par variables 
                            par->stage = i;
                            par->performance_state = 1;
                            par->stage_type = 0;
                            par->singer_mode = 2;
                            pthread_mutex_unlock(&(par->participant_lock));
                            //release par lock
                            pthread_mutex_lock(&(list_of_acoustic_stages[i]->stage_lock));
                            //acquire stage lock and change the singer id
                            list_of_acoustic_stages[i]->singer_id = par->id;
                            pthread_mutex_unlock(&(list_of_acoustic_stages[i]->stage_lock));
                            //release stage lock
                            sem_post(&(par->sem_par)); 
                            pthread_exit(NULL);
                        }
                    }
                }
            }

            //couldn't acquire any of the acoustic stages (not solo nor duo) so try electric stages
            for (int j = 0; j < num_electric_stages ; j++)
            {
                es = sem_trywait(&(list_of_electric_stages[j]->sem_par_space));
                if (es == 0)
                {
                    //Electric stage found
                    //acquire par lock
                    pthread_mutex_lock(&(par->participant_lock));
                    //if par has left sem_post immediately
                    if(par->performance_state == -1) 
                    {
                        sem_post(&(list_of_electric_stages[j]->sem_par_space));
                        //this means we didnt use the stage at all and exited
                        pthread_mutex_unlock(&(par->participant_lock));
                        //release par lock
                        pthread_exit(NULL);
                    }
                    else
                    {
                        //dont sempost (sempost in participant thread function after performance)
                        //update par variables 
                        par->stage = j;
                        par->performance_state = 1;
                        par->stage_type = 1;
                        pthread_mutex_unlock(&(par->participant_lock));
                        //release par lock
                        sem_post(&(par->sem_par)); 
                        pthread_exit(NULL);
                    }
                }
                else if (es == -1) 
                {
                    //Acoustic stage not found as a solo performer
                    esmu = sem_trywait(&(list_of_electric_stages[j]->sem_sing_space));
                    if (esmu == 0)
                    {
                        //Acoustic stage found as a duo performer
                        //acquire par lock
                        pthread_mutex_lock(&(par->participant_lock));
                        //if par has left sem_post immediately
                        if(par->performance_state == -1) 
                        {
                            sem_post(&(list_of_electric_stages[j]->sem_sing_space));
                            //this means we didnt use the stage at all and exited
                            pthread_mutex_unlock(&(par->participant_lock));
                            //release par lock
                            pthread_exit(NULL);
                        }
                        else
                        {
                            //dont sempost (sempost in participant thread function after performance)
                            //update par variables 
                            par->stage = j;
                            par->performance_state = 1;
                            par->stage_type = 0;
                            par->singer_mode = 2;
                            pthread_mutex_unlock(&(par->participant_lock));
                            //release par lock
                            pthread_mutex_lock(&(list_of_electric_stages[j]->stage_lock));
                            //acquire stage lock and change the singer id
                            list_of_electric_stages[j]->singer_id = par->id;
                            pthread_mutex_unlock(&(list_of_electric_stages[j]->stage_lock));
                            //release stage lock
                            sem_post(&(par->sem_par)); 
                            pthread_exit(NULL);
                        }
                    }
                }
            }

            //couldnt acquire any electric stages too so now go through all of them again
        }
    }

}







//---------------------Init and Other Normal Functions--------------------------------



int init_participant(int id,char* name,char instrument, int arrival_time)
{
    list_of_participants[id] = (struct participant *)malloc(sizeof(struct participant));
    list_of_participants[id]->id = id;
    list_of_participants[id]->name = name;
    list_of_participants[id]->instrument = instrument;
    list_of_participants[id]->arrival_time = arrival_time;
    list_of_participants[id]->stage_type = -1;
    list_of_participants[id]->singer_mode = 0;
    list_of_participants[id]->performance_state = 0;
    list_of_participants[id]->stage = -1;

    sem_init(&list_of_participants[id]->sem_par, 0, 0);
    pthread_mutex_init(&list_of_participants[id]->participant_lock, NULL); 

    return 0;
}


int init_acoustic_stages(int id)
{
    list_of_acoustic_stages[id] = (struct stage *)malloc(sizeof(struct stage));
    list_of_acoustic_stages[id]->id = id;
    list_of_acoustic_stages[id]->singer_id = -1;
    list_of_acoustic_stages[id]->participant_id = -1;
    pthread_mutex_init(&list_of_acoustic_stages[id]->stage_lock, NULL); 
    sem_init(&list_of_acoustic_stages[id]->sem_par_space, 0, 1);    //initially there will be 1 space for a participant to perform
    sem_init(&list_of_acoustic_stages[id]->sem_sing_space, 0, 1);   //initially there will be 1 space for a singer to perform
}


int init_electric_stages(int id)
{
    list_of_electric_stages[id] = (struct stage *)malloc(sizeof(struct stage));
    list_of_electric_stages[id]->id = id;
    list_of_electric_stages[id]->singer_id = -1;
    list_of_electric_stages[id]->participant_id = -1;
    pthread_mutex_init(&list_of_electric_stages[id]->stage_lock, NULL); 
    sem_init(&list_of_electric_stages[id]->sem_par_space, 0, 1);    //initially there will be 1 space for a participant to perform
    sem_init(&list_of_electric_stages[id]->sem_sing_space, 0, 1);   //initially there will be 1 space for a singer to perform
}


char ** tokenizer(char *input_line, char *delimiter)    //Old code from previous assignment
{
    
    char **alltokens = malloc(256 * sizeof(char *));
    if (alltokens == NULL) {
        perror("malloc failed");
        exit(1);
    }  
    char *token;
    int position = 0;

    token = strtok(input_line, delimiter);
    while(token != NULL) {
        alltokens[position] = token;
        position++;

        token = strtok(NULL, delimiter);
    }
    alltokens[position] = NULL;
    return alltokens;
}






/*

Test Case 1

4 2 1 2 5 7 2

aditya g 1
prajwal b 1
Malay s 1
ankit g 1



Test Case 2

4 2 1 2 5 7 8

aditya g 1
prajwal b 1
Malay s 1
ankit g 3



*/



//TODO : 

//*DONE singer wala nahi kaam krr raha hai 
//*DONE Timeout hone prr kya karna hai wo bhi likhna hai
//* DONE : Co-ordinator wala semaphore ka bhi kaam karna hai 


