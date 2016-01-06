// Copyright (c) 2013 Grigory Demchenko

#include "helpers.h"

void test();
void test2();
void replob();

int main(int argc, char* argv[])
{
    try
    {
        //test2();
        replob();
    }
    catch (std::exception& e)
    {
        RLOG("Error: " << e.what());
        return 1;
    }
    catch (...)
    {
        RLOG("Unknown error");
        return 2;
    }
    RLOG("main ended");
    return 0;
}
