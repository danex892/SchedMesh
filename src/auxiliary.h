#ifndef AUXILIARY_H
#define AUXILIARY_H

#include "schedmesh.h"

using namespace std;

string remove_surrounding_quotes(string &s);
string trim(string s);
bool file_exists(const string &name);
string get_available_fname(string &fname);

int calc_winds(Teacher &t, int d, int lessons);
void get_subject_num(Group &g, int tid, int tid1, string &name, string &name1, int &n1, int &n2);
int get_classroom_of_teacher(Teacher &t, int tid, int d, int l);
bool is_double(string &x);
bool check_first_last(Group &g, int t, int l, Settings &stn);
bool check_conflicts(Group &g, int t, int d, int l);
bool check_physical_culture(Group &g, Gym &gym, int t, int d, int l);
bool check_double_lesson(Group &g, Teacher &t, int s, int d, int l);
bool check_profile_lesson(Group &g, vector<Teacher> &t, int t1, int &t1s2, int s, int &s1, int d, int l, int randtype);
void move_teacher_from_to_room(vector<Classroom> &r, int d, int l, int t1, int r1, int r2);
bool check_room(vector<Classroom> &rooms, vector<Teacher> &t, int t1, int d, int l, int &res_r, int &res_r1,
                int &res_t2, int arr_r[], int cnt);
bool improve_timetable(vector<Group> &g, vector<Teacher> &t, vector<Classroom> &rooms, Gym &gym, Settings &stn);
void optimize_timetable(vector<Group> &g, vector<Teacher> &t, vector<Classroom> &rooms, Gym &gym, Settings &stn);

#endif // AUXILIARY_H
