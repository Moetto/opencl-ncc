#include "timing.h"
#include <sys/time.h>
#include <iostream>
#include <iomanip>
#include <sstream>

Timer::Timer() {
    checkPoints = vector<timeStamp>();
}

void Timer::start() {
    timeval t;
    gettimeofday(&t, NULL);
    startTime = timeStamp("Start", t);
}

void Timer::stop() {
    gettimeofday(&endTime.time, NULL);
    outputDiff(startTime, endTime);
}

void Timer::checkPoint(string name) {
    timeval check;
    gettimeofday(&check, NULL);
    timeStamp stamp = {name, check};
    if (checkPoints.size() > 0)
        outputDiff(checkPoints.back(), stamp);
    else {
        outputDiff(startTime, stamp);
    }
    checkPoints.push_back(stamp);
}

void Timer::outputDiff(timeStamp t1, timeStamp t2) {
    long secs, usecs;
    long long diff;
    int million = 1000000;
    diff = t2.time.tv_sec * million - t1.time.tv_sec * million + t2.time.tv_usec - t1.time.tv_usec;
    secs = diff / million;
    usecs = diff - secs * million;
    std::stringstream s = std::stringstream();
    s << secs << "." << usecs;

    std::cout << std::left << std::setfill('0') << std::setw(8) << s.str() << "s";
    std::cout << std::setfill(' ') << " " << std::setw(30) << t1.name;
    std::cout << std::setw(30) << t2.name << std::endl;
}
