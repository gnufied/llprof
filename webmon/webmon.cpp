
#include "http.h"
#include "data_store.h"
#include <string>

#include <iostream>

using namespace std;


int main()
{
    InitDataStore();
    start_http_server();
    cout << "Webserver start." << endl;
    
    while(true)
    {
        sleep(10);
    }
}

