
#include "http.h"
#include "data_store.h"
#include "../llprofcommon/network.h"
#include <string>

#include <iostream>

using namespace std;
using namespace llprof;


void ph(string s)
{
    cout << "Hash:" << s << " => " << HashStr(s) << endl;
    
}

int main()
{
    /*
    ph("visit_Attribute");
    ph("_clone");
    ph("c");
    ph("__init__");
    ph("init____");
    ph("init");
    ph("tini");
    ph("gg");
    */
    InitSocketSubSystem();
    
    InitDataStore();
    start_http_server();
    cout << "Webserver start." << endl;
    
    while(true)
    {
        sleep(10);
    }
}

