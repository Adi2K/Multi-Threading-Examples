#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>


// Just a function that gives minimum of 2 numbers

#define min(a,b)             \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b;       \
})



//------------------Function Declerations-------------------------------

int init_pharma_company(int id,int sucess_perc,int batches_prepared);
int init_student(int id);
int init_vaccination_zone(int id);


int vaccines_ready_distributing(int num);
int zone_making_slots_avaliable(int input,int vaccines_recieved);


void *vaccination_zone_thread_func(void *arg);
void *phrama_company_thread_func(void *arg);
void *student_thread_func(void *arg);

int checktotalstudents();
int decrementstudents();


int print_pharma_details(int id);  //debugging mei use kiya thaa

//------------------------------------------------------------------------






//-------------------Structs------------------------------------------------


typedef struct pharma_company
{
    int id;
    int success_percent;
    int batches_prepared;
    pthread_mutex_t lock;
    pthread_cond_t pharma_cond;

}pharma_company;

typedef struct student
{   
    int roll;  // roll number 
    int vaccines_recieved;  //number of vaccines that he recieved
    int has_antibody;  //did he get the anti-body
    pthread_mutex_t lock;

}student;


typedef struct vaccination_zone 
{
    pthread_mutex_t lock;      // jbb vaccination phase mei hoga toh lock bnnd rahega toh uske liye seperate var ki zaroorat nai hai
    pthread_cond_t vaccz_cond;
    int zone_id;
    int has_batch;
    int recieved_vaccine_potency;
    int vaccines_left;
    int slots_avaliable;

} vaccination_zone;






//---------------------------------------------------------------------------




//----------------------Global Variables-------------------------------------

pharma_company *list_of_pharma_company[200];
student *list_of_student[200];
vaccination_zone *list_of_vaccination_zone[200];

int total_students;
pthread_mutex_t total_students_lock;
  
int total_vaccination_zones;
int total_pharma_companies;


//----------------------------------------------------------------------------




//x-x-x-x-x-x-x-x-x-x-x----------MAIN FUNCTION------------x-x-x-x-x-x-x-x-x-x-x-x




int main()
{   

    //---------------Pharma Companies ka Input aur Initiation-----------------
    pthread_mutex_init(&total_students_lock, NULL);   //total  students wala mutex lock
    int num_pharma;
    srand(time(NULL)); 
    printf("Enter the number of Pharma Companies : ");
    scanf("%d",&num_pharma);
    total_pharma_companies = num_pharma;
    printf("\n");
    for (int i = 0 ; i<num_pharma ; i++)
    {   
        int sucess_perc;
        int batches_prepared = 1+rand()%5;
        printf("Enter the Percentage of Success of Pharma %d : ",i);
        scanf("%d",&sucess_perc);
        int id = i;
        init_pharma_company(id,sucess_perc,batches_prepared);
    }
    /*printf("\n\nPrinting Pharma Details : \n");
    for (int i = 0 ; i<num_pharma ; i++)
    {
        print_pharma_details(i);
        printf("\n");
    } */
    //------------------------------------------------------------------------





    //-------------------Students ka input aur Initiation----------------------
    int  num_student;
    printf("\nEnter the number of Students : ");
    scanf("%d",&num_student);
    total_students = num_student ;
    for (int i = 0; i<num_student; i++)
    init_student(i);

    //------------------------------------------------------------------------


    //-------------------Vaccination ZOnes ka Input aur Initiation------------
    int num_vaccination_zone;
    printf("\nEnter the number of Vaccination Zones : ");
    scanf("%d",&num_vaccination_zone);
    total_vaccination_zones = num_vaccination_zone;
    for (int i = 0;i<num_vaccination_zone;i++)
    init_vaccination_zone(i);


    //------------------------------------------------------------------------
    



    //--------------Pharma Company ka Thread Creation---------------------------
    pthread_t *pharma_threads = (pthread_t*)malloc(sizeof(pthread_t)*num_pharma);

    int iter = 0;


    int *nums_iter = (int*) malloc(num_pharma * sizeof(int));  // We are creating a array of numbers to refrence their addersses because we cant refrence the addresses of iterator
                                                              //If we dont use this technique then :
                                                             //As soon as this loop exists, all your threads are then pointing at a variable which no longer exists.
                                                            //So when they dereference their pointer, the value they get (if they get anything at all) is anyones guess.
    for (iter = 0;iter<num_pharma;iter++)
    {
        nums_iter[iter] = iter;
    }
    

    for (iter = 0;iter<num_pharma;iter++)
    {
        pthread_create(&pharma_threads[iter], NULL, &phrama_company_thread_func, (void*)(&nums_iter[iter])); 
    }
    //--------------End of Pharma Company ka Thread Creation--------------------


    sleep(6);// required bcoz if we shouldnt start distribution unless all the companies are ready to supply (company takes max 6 sec to produce vacc)



    //--------------Vaccination zone ka Thread Creation---------------------------


    pthread_t *vacc_zone_threads = (pthread_t*) malloc(sizeof(pthread_t)*num_vaccination_zone);

    int it = 0;
    int *nums_it = (int*) malloc(num_vaccination_zone * sizeof(int));

    for (it = 0;it<num_pharma;it++)
    {
        nums_it[it] = it;
    }


    for  (it = 0;it<num_vaccination_zone;it++)
    {
        pthread_create(&vacc_zone_threads[it], NULL, &vaccination_zone_thread_func, (void*)(&nums_it[it])); 
        //sleep(1);
    }

    //--------------End of Vaccination Zone ka Thread Creation--------------------




    //--------------Student ka Thread Creation---------------------------
    pthread_t *student_threads = (pthread_t*) malloc(200*sizeof(pthread_t)*num_student);
    int i;
    int *nums_i = (int*) malloc(num_student * sizeof(int));


    for (i = 0;i<num_student;i++)
    {
        nums_i[i] = i;
    }

    for  (i = 0;i<num_student;i++)
    {
        pthread_create(&student_threads[i], NULL, &student_thread_func, (void*)(&nums_i[i])); 
        sleep(1);
    } 


    
    //---------------Thread Joining-------------------------------------------



    // All of these are non-ending threads and will never join we basically end when the number of students is 0
    //This is done in  checktotalstudents() and  decrementstudents() functions

    int jiter;
    for (jiter = 0;jiter<num_pharma;jiter++)
    {
        pthread_join(pharma_threads[jiter],NULL);
    } 


    //-----------------------------------------------------------------------
    printf("\n**This should never be printed**\n");
    

}



//-x-x-x-x-x-x-x-x-x-x-x-x-----END OF MAIN-------------x-x-x-x-xx-x--x-x-x-x-x-x-x-x-x-x



//---------------------- Pharma companies ke functions------------------------------------------
int init_pharma_company(int id,int sucess_perc,int batches_prepared)
{
    list_of_pharma_company[id] = (struct pharma_company *)malloc(sizeof(struct pharma_company));
    list_of_pharma_company[id]->id = id;
    list_of_pharma_company[id]->success_percent = sucess_perc;
    list_of_pharma_company[id]->batches_prepared = batches_prepared;
    pthread_mutex_init(&list_of_pharma_company[id]->lock, NULL); 
    pthread_cond_init(&list_of_pharma_company[id]->pharma_cond, NULL);   

    return 0;
}

int print_pharma_details(int id)
{
    printf("Company id : %d\n",list_of_pharma_company[id]->id);
    printf("Company Batches Prepared : %d\n",list_of_pharma_company[id]->batches_prepared);
    printf("Success Percentage of Vaccine : %d\n",list_of_pharma_company[id]->success_percent);
}




void *phrama_company_thread_func(void *arg)
{
    int inp = *(int *)arg;
    pthread_mutex_lock(&list_of_pharma_company[inp]->lock);
    while ((int)checktotalstudents() > 0)
    { 
        printf("\n\nEntered Production Phase\n\n");
        int preparetime = 2 + rand() % 4;
        list_of_pharma_company[inp]->batches_prepared = 1+rand()%5;
        printf("Pharmaceutical Company %d is preparing %d batches of vaccines which have success probability %d\n\n",inp,list_of_pharma_company[inp]->batches_prepared,list_of_pharma_company[inp]->success_percent);
        sleep(preparetime);

        printf("Pharmaceutical Company %d has prepared %d batches of vaccines which have success probability %d. Waiting for all the vaccines to be used to resume production\n\n",inp,list_of_pharma_company[inp]->batches_prepared,list_of_pharma_company[inp]->success_percent);
        
        vaccines_ready_distributing(inp);
    }
    pthread_mutex_unlock(&list_of_pharma_company[inp]->lock);
    printf("\n**This Should Never be Printed**\n");
    pthread_exit(NULL);
}


int vaccines_ready_distributing(int num)
{
    while (list_of_pharma_company[num]->batches_prepared != 0)
    {
        pthread_cond_wait(&list_of_pharma_company[num]->pharma_cond, &list_of_pharma_company[num]->lock);
        list_of_pharma_company[num]->batches_prepared--;
    }
}
    



//----------------------------------------------------------------------------------------------


//----------------------Student ke Functions-----------------------------------------------------
int init_student(int id)
{   
    list_of_student[id] = (struct student *)malloc(sizeof(struct student));
    list_of_student[id]->roll = id;
    list_of_student[id]->vaccines_recieved = 0;
    list_of_student[id]->has_antibody = 0;
    pthread_mutex_init(&list_of_student[id]->lock, NULL); 
    return 0;
}

void *student_thread_func(void *arg)
{
    int inpu = *(int *)arg;
    pthread_mutex_lock(&list_of_student[inpu]->lock);
    int anti_bd = 0, vacc_rec = 0;
    int zone = rand() % total_vaccination_zones;
    printf("Student %d assigned a slot on the Vaccination Zone %d and waiting to be vaccinated\n\n",inpu,zone);

    while (list_of_student[inpu]->has_antibody == 0 && list_of_student[inpu]->vaccines_recieved <3)
    { 
        pthread_cond_signal(&list_of_vaccination_zone[zone]->vaccz_cond);
        printf("Student %d on Vaccination Zone %d has been vaccinated which has success probability %d\n\n",inpu,zone,list_of_vaccination_zone[zone]->recieved_vaccine_potency);
        
        if ( rand() % 100 < list_of_vaccination_zone[zone]->recieved_vaccine_potency)
        {
            printf("Found antibodies in Student %d \n\n",inpu);
            list_of_student[inpu]->has_antibody =1;
            list_of_student[inpu]->vaccines_recieved++;
            /* anti_bd = 1; */
        }
        else
        {
            printf("Didnt find antibodies in Student %d \n\n",inpu);
            list_of_student[inpu]->vaccines_recieved++;
            /* vacc_rec ++; */
        }
    }

    if (list_of_student[inpu]->has_antibody == 0)
    {
        printf("Student %d will have to attend online semester\n",inpu);
    }
    else
    {
        printf("Student %d will be joining campus\n",inpu);
    }
    decrementstudents();
}




//-----------------------------------------------------------------------------------------------




//------------------Vaccination Zone ke functions----------------------------------------------
int init_vaccination_zone(int id)
{   
    list_of_vaccination_zone[id] = (struct vaccination_zone *)malloc(sizeof(struct vaccination_zone));
    list_of_vaccination_zone[id]->zone_id = id;
    list_of_vaccination_zone[id]->has_batch = 0;
    list_of_vaccination_zone[id]->vaccines_left = 0;
    list_of_vaccination_zone[id]->slots_avaliable = 0;
    pthread_mutex_init(&list_of_vaccination_zone[id]->lock, NULL);           
    pthread_cond_init(&list_of_vaccination_zone[id]->vaccz_cond, NULL);   
//    pthread_cond_init(&list_of_vaccination_zone[id]->company_cond, NULL);
    return 0;
}



void *vaccination_zone_thread_func(void *arg)
{
    int input = *(int *)arg;
    pthread_mutex_lock(&list_of_vaccination_zone[input]->lock);
    while (checktotalstudents() > 0 )
    { 
        int vaccines_recieved;
        int company_id = rand() % total_pharma_companies;  //selecting a company at random to take vaccines from
        pthread_cond_signal(&list_of_pharma_company[company_id]->pharma_cond);
        vaccines_recieved = 10 + rand() %10;
        list_of_vaccination_zone[input]->recieved_vaccine_potency = list_of_pharma_company[company_id]->success_percent;
        printf("Pharmaceutical Company %d has delivered vaccines to Vaccination zone %d, alloting slots now\n\n",company_id,input);
        //alott slots
        zone_making_slots_avaliable(input,vaccines_recieved);
        
    }
    pthread_mutex_unlock(&list_of_vaccination_zone[input]->lock);
    pthread_exit(NULL);
}



int zone_making_slots_avaliable(int input,int vaccines_recieved)
{
    int vaccines_remaining = vaccines_recieved;
    while (vaccines_remaining > 0 && checktotalstudents()>0)
    {
        int slots_avaliable = min(min(8,vaccines_remaining),checktotalstudents());
        vaccines_remaining = vaccines_remaining - slots_avaliable;
        printf("Vaccination Zone %d is ready to vaccinate with %d slots \n\n",input,slots_avaliable);
        printf("Vaccination Zone %d entering Vaccination Phase\n\n",input);
        while(slots_avaliable >0)
        {
            pthread_cond_wait(&list_of_vaccination_zone[input]->vaccz_cond, &list_of_vaccination_zone[input]->lock);
            slots_avaliable--;
        }
    }
}

//------------------Total Students ke functions----------------------------------------------

int checktotalstudents()
{
    int status = pthread_mutex_lock(&total_students_lock);
    assert(status == 0);
    int result = total_students;
    if(result <=0)
    {
        printf("\n\nSimulation Over\n");
        exit(0);
    }
    //printf("\nChecking .... : There are %d Student(s) \n",result);
    //assert(result>=0);
    status = pthread_mutex_unlock(&total_students_lock);
    assert(status == 0);
    return result;
}


int decrementstudents()
{
    int status = pthread_mutex_lock(&total_students_lock);
    assert(status == 0);
    int result = total_students;
    //printf("\nDecrementing .... : There are %d Student(s) \n",result);
    result--;
    //printf("\nAfter Decrementing : There are %d Student(s) \n",result);
    if(result <=0)
    {
        printf("\n\nSimulation Over\n");
        exit(0);
    }
    total_students = result;
    status = pthread_mutex_unlock(&total_students_lock);
    assert(status == 0);
    return result;
}
//----------------------------------------------------------------------------------------------