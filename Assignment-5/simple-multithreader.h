#ifndef SIMPLE_MULTITHREADER_H
#define SIMPLE_MULTITHREADER_H

#include <iostream>
#include <functional>
#include <stdlib.h>
#include <cstring>
#include <pthread.h>
#include <chrono>

struct ThreadArgs1D{
    int low;                             // Starting index
    int high;                            // Ending index (excluded)
    std::function<void(int)> *lambda_ptr;//ptr to the lambda function
};

struct ThreadArgs2D{
    int low1;                                 
    int high1;                                //outer loop ending index (excluded)
    int low2;                                 
    int high2;                                //inner loop ending index (excluded)
    std::function<void(int, int)> *lambda_ptr;// ptr to the lambda func
};


void* worker_1d(void* arg){
    ThreadArgs1D* args= (ThreadArgs1D*)arg;
    
    for(int i =args->low;i<args->high;++i){  // execute lambda for each index in the assigned range
        (*(args->lambda_ptr))(i);
    }
    return NULL;
}


void* worker_2d(void* arg){
    ThreadArgs2D* args=(ThreadArgs2D*)arg;
    // execute lambda for each (i,j) in the assigned range
    for(int i=args->low1;i < args->high1;++i){
        for(int j=args->low2;j < args->high2;++j){
            (*(args->lambda_ptr))(i, j);
        }
    }
    
    return NULL;
}

void parallel_for(int low, int high, std::function<void(int)> &&lambda, int numThreads){
    // Input validation
    if(high <= low){
        std::cerr << "error: Invalid range["<< low << ", " << high << ")" << std::endl;
        return;
    }
    
    if(numThreads <= 0){
        std::cerr << "warning:invalid numThreads (" << numThreads << "), setting to 1" << std::endl;
        numThreads=1;
    }
    
    auto start_time= std::chrono::high_resolution_clock::now();

    
    int num_workers=numThreads-1;  // calculate number of worker threads (main thread will be the nth thread)
    pthread_t* threads= nullptr; // dynamically allocate arrays for threads and args
    ThreadArgs1D* args= nullptr;
    
    try{
        threads= new pthread_t[num_workers];
        args= new ThreadArgs1D[numThreads];
    } 
    
    catch(std::bad_alloc& e){
        std::cerr << "error: memory allocation failed: " << e.what() << std::endl;
        delete[] threads;
        delete[] args;
        return;
    }

    
    int total_elements= high-low;  //distribution
    int chunk_size= total_elements / numThreads;
    int remainder= total_elements % numThreads;

    int current_low= low;

    // Launch Worker Threads
    // Each thread gets chunk_size elements, with first 'remainder' threads getting +1
    for (int i=0;i < num_workers;++i){
        // int my_chunk= chunk_size+ (i < remainder ? 1 : 0);


        int my_chunk=chunk_size;

        if(remainder){
            my_chunk++;
            remainder--;}
        
        args[i].low= current_low;
        args[i].high= current_low + my_chunk;
        args[i].lambda_ptr= &lambda;
        
        current_low+= my_chunk;
        
        int rc= pthread_create(&threads[i], NULL, worker_1d, (void*)&args[i]);

        if(rc){
            std::cerr << "error: unable to create thread " << i << ", error code: " << rc << std::endl;
            //join already created threads before cleanup
            for (int j= 0;j<i;++j){
                pthread_join(threads[j], NULL);
            }
            
            delete[] threads;
            delete[] args;
            return;
        }
    }


    int my_chunk=chunk_size; // main thread executes the last chunk

        if(remainder){
            my_chunk++;
            remainder--;
        }
    args[num_workers].low= current_low;
    args[num_workers].high= current_low + my_chunk;
    args[num_workers].lambda_ptr= &lambda;
    
    // executes main thread's portion directly
    worker_1d((void*)&args[num_workers]);

    for (int i= 0;i< num_workers;++i){
        int rc= pthread_join(threads[i], NULL);
        if(rc){
            std::cerr << "error:unable to join thread "<< i << ", error code: " << rc << std::endl;
        }
    }

    //cleanup
    delete[] threads;
    delete[] args;

    auto end_time= std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff= end_time-start_time;
    std::cout<< "Parallel execution time: " <<diff.count() << " s\n";
}

void parallel_for(int low1, int high1, int low2, int high2,std::function<void(int, int)> &&lambda, int numThreads) {
    // Input validation
    if (high1<= low1){
        std::cerr << "Error: Invalid outer range [" << low1 << ", " << high1 << ")" << std::endl;
        return;
    }
    
    if (high2<= low2){
        std::cerr << "Error: Invalid inner range [" << low2 << ", " << high2 << ")" << std::endl;
        return;
    }
    
    if (numThreads<= 0){
        std::cerr << "Warning: Invalid numThreads (" << numThreads << "), setting to 1" << std::endl;
        numThreads=1;
    }

    auto start_time= std::chrono::high_resolution_clock::now();


    int num_workers= numThreads-1;     // Calculate number of worker threads
    pthread_t* threads= nullptr;     // dynamically allocated arrays for threads and arguments
    ThreadArgs2D* args= nullptr;

    
    try{
        threads= new pthread_t[num_workers];
        args= new ThreadArgs2D[numThreads];
    } 
    
    catch (std::bad_alloc& e){
        std::cerr << "error: Memory allocation failed: "<< e.what() << std::endl;
        delete[] threads;
        delete[] args;
        return;
    }

    // We parallelize the rows (outer loop) across threads
    int total_rows= high1-low1;
    int chunk_size= total_rows / numThreads;
    int remainder= total_rows % numThreads;

    int current_low1= low1;

    // Launch Worker Threads
    for(int i=0;i< num_workers;++i){
        int my_chunk=chunk_size;

        if(remainder){
            my_chunk++;
            remainder--;
        }
        
        args[i].low1= current_low1;
        args[i].high1= current_low1 + my_chunk;
        args[i].low2= low2;  // Full range for inner loop
        args[i].high2= high2;// Full range for inner loop
        args[i].lambda_ptr= &lambda;

        current_low1+= my_chunk;

        int rc= pthread_create(&threads[i], NULL, worker_2d, (void*)&args[i]);


        if(rc){
            std::cerr<< "error: unable to create thread " << i << ", error code: " << rc << std::endl;
            
            // Join already created threads before cleanup
            for(int j=0;j < i;++j){
                pthread_join(threads[j], NULL);
            }
            
            delete[] threads;
            delete[] args;
            return;
        }
    }

    int my_chunk=chunk_size; //main thread's chunk

    if(remainder){
        my_chunk++;
        remainder--;
    }

    args[num_workers].low1= current_low1;
    args[num_workers].high1= current_low1 + my_chunk;
    args[num_workers].low2= low2;
    args[num_workers].high2= high2;
    args[num_workers].lambda_ptr= &lambda;

    
    worker_2d((void*)&args[num_workers]);   // Execute main thread's portion directly


    for(int i= 0;i< num_workers;++i){   //worker threads
        int rc= pthread_join(threads[i], NULL);
        
        if(rc){
            std::cerr << "Error: Unable to join thread " << i << ", error code: " << rc << std::endl;
        }
    }

    // Cleanup dynamically allocated memory
    delete[] threads;
    delete[] args;

    auto end_time= std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff= end_time-start_time;
    std::cout << "Parallel execution time: " << diff.count() << " s\n";
}



int user_main(int argc, char **argv);

void demonstration(std::function<void()> && lambda){
  lambda();
}

int main(int argc, char **argv){

  int x=5,y=1;
  // Declaring a lambda expression that accepts void type parameter
  auto /*name*/ lambda1=/*capture list*/[/*by value*/ x, /*by reference*/ &y](void) {
    /* Any changes to 'x' will throw compilation error as x is captured by value */
    y=5;
    std::cout<<"====== Welcome to Assignment-"<<y<<" of the CSE231(A) ======\n";
    /* you can have any number of statements inside this lambda body */
  };
  // Executing the lambda function
  demonstration(lambda1);// the value of x is still 5, but the value of y is now 5

  int rc=user_main(argc, argv);
 
  auto /*name*/ lambda2=[/*nothing captured*/]() {
    std::cout<<"====== Hope you enjoyed CSE231(A) ======\n";
    /* you can have any number of statements inside this lambda body */
  };
  demonstration(lambda2);
  return rc;
}

#define main user_main

#endif
