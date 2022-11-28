#include <omp.h>
#include <iostream>
#include <unistd.h>
#include <thread>
#include <memory>
#include <vector>
#include <unistd.h>
#include "window.h"
#define THREAD_NUM 3

using namespace sphexa;

std::shared_ptr<Window> w = std::make_shared<Window>(800, 600, "SPHEXA Thread computation visualization");

bool        count[THREAD_NUM] = {false};
bool        count_prev[THREAD_NUM] = {false};
std::mutex count_mutex;

void computation(void)
{
    omp_set_num_threads(THREAD_NUM); // set number of threads in "parallel" blocks
    int t = 2;
#pragma omp parallel
    {

        sleep(t*omp_get_thread_num());
        #pragma omp critical
        {
            count_mutex.lock();
            std::cout << "Current thread number: " << omp_get_thread_num() << std::endl;
            count[omp_get_thread_num()] = true;
            
            count_mutex.unlock();
        }
    }
}

void ui(void)
{
    w->init(THREAD_NUM);
    w->loop(count, count_prev, count_mutex);
}

int main()
{
    std::thread computationThread(computation);
    std::thread uiThread(ui);

    computationThread.join();
    uiThread.join();

    return 0;
}