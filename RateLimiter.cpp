#include <chrono>
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <string>
#include <memory>
#include <shared_mutex>


double MAX_TOKENS = 5;
double TOKEN_REFILL_RATE = 2;
struct TokenBucket {
    std::mutex mtx;
    double current_tokens;
    std::chrono::steady_clock::time_point last_refill_time;
    
    TokenBucket(){
        current_tokens = MAX_TOKENS;
        last_refill_time = std::chrono::steady_clock::now();
    }

    bool allow(){


        std::lock_guard<std::mutex> bucket_lock(mtx);

        auto current_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = current_time - last_refill_time;
        double elapsed_time = elapsed.count();

        current_tokens = std::min(MAX_TOKENS, current_tokens + elapsed_time * TOKEN_REFILL_RATE);

        last_refill_time = current_time;
        
        if (current_tokens >= 1){
            current_tokens -= 1;
            return true;
        };

        return false;


    };

};


struct RateLimiter {

    /*

    rate limiter:
        allow function -> takes in user id: 
            -checks if user id is in mapping
                -if so, call the allow function

                -if not, create a new mapping item and then call allow

        how to handle concurrency:
            -any amount of threads can read from the block, but only one thread can write, and readers must wait while write is happening (a write can cause the hashmap to reallocate and shift things around)
            -

    */
    std::shared_mutex map_mutex;
    //need a pointer to the token bucket because mutexes aren't copyable
    std::unordered_map<std::string, std::shared_ptr<TokenBucket>> buckets;


    bool allow(std::string user_id) {

        //reader lock required to check map (in case it's in the middle of reallocating itself)


        
        {
            std::shared_lock<std::shared_mutex> lock_shared(map_mutex);

            if (buckets.find(user_id) != buckets.end()) {

                //user already has a bucket, so call bucket->allow on that
                return buckets[user_id]->allow();

            }  
        
        }

        std::unique_lock<std::shared_mutex> unique_lock(map_mutex);


        if (buckets.find(user_id) == buckets.end()) {
            buckets[user_id] = std::make_shared<TokenBucket>();

        }

        return buckets[user_id]->allow();

    }





};