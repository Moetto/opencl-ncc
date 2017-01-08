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
    outputDiff(startTime, endTime, startTime);
}

void Timer::checkPoint(string name) {
    timeval check;
    gettimeofday(&check, NULL);
    timeStamp stamp = {name, check};
    if (checkPoints.size() > 0)
        outputDiff(checkPoints.back(), stamp, startTime);
    else {
        outputDiff(startTime, stamp, startTime);
    }
    checkPoints.push_back(stamp);
}

string diff(timeval t1, timeval t2) {
    long secs, usecs;
    long diff;
    long million = 1000000;
    diff = (t2.tv_sec - t1.tv_sec) * million + t2.tv_usec - t1.tv_usec;
    secs = diff / million;
    usecs = diff - secs * million;
    std::stringstream s = std::stringstream();
    s << std::setw(2) << secs << "." << std::setfill('0') << std::setw(6) << usecs;
    return s.str();
}

void Timer::outputDiff(timeStamp t1, timeStamp t2, timeStamp start) {
    std::cout << diff(t1.time, t2.time) << "s ";
    std::cout << diff(start.time, t2.time) << "s";
    std::cout << std::setfill(' ') << " " << std::setw(30) << t1.name;
    std::cout << std::setw(30) << t2.name << std::endl;
}
