#ifndef PRINTDATA_H
#define PRINTDATA_H

#include "schedmesh.h"

using namespace std;

void print_teachers_winds(vector<Teacher> &t, Settings &stn);
void print_schedule_pupils(vector<Group> &g, Settings &stn);
void print_schedule_teachers(vector<Teacher> &t, Settings &stn);
void print_schedule_rooms(vector<Teacher> &t, vector<Classroom> &r, Settings &stn);
void print_schedule_gym(Gym &gym, Settings &stn);
void print_needed_lessons(vector<Group> &g, vector<Teacher> &t, Settings &stn);
void print_full_info(vector<Group> &classes, vector<Teacher> &teachers, vector<Classroom> &rooms, Gym &gym,
                     Settings &stn, int step, int bugstep, int brokendays, int type);

#endif // PRINTDATA_H
