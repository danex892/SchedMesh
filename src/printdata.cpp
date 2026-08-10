#include "schedmesh.h"

void print_teachers_winds(vector<Teacher> &t, Settings &stn)
{
    int winds = 0;
    int hours = 0;
    int maxwind = 0;
    int maxwindday = 0;
    double expectedval = 0;
    double dispers = 0;

    int A[100] = {0};

    bool iscalculated = false;
    for (vector<Teacher>::iterator it = t.begin(); it != t.end(); ++it)
    {
        if (it->name != "" && it->hours != 0)
        {
            iscalculated = true;
            break;
        }
    }

    if (iscalculated)
    {
        cout << "Teacher;Hours;Empty slots" << endl;

        int p = 1;
        for (vector<Teacher>::iterator it = t.begin(); it != t.end(); ++it)
        {
            if (it->name != "")
            {
                cout << it->name << ";" << it->hours << ";" << it->totalwinds << endl;
                hours += it->hours;
                A[p - 1] = it->totalwinds;
                if (A[p - 1] > maxwind)
                {
                    maxwind = A[p - 1];
                }
                winds += A[p - 1];
                for (int q = 0; q < stn.days; q++)
                {
                    if (it->winds[q] > maxwindday)
                    {
                        maxwindday = it->winds[q];
                    }
                }
            }
            if (p == stn.countteachers) break;
            p++;
        }

        expectedval = 1.0 * winds / stn.countteachers;

        for (int tt = 0; tt < stn.countteachers; tt++)
        {
            dispers += pow(A[tt] - expectedval, 2);
        }
        dispers /= stn.countteachers;

        cout << "Total empty slots: " << winds << endl;
        cout << "Total hours: " << hours << endl;
        cout << "Mean of empty slots: " << expectedval << endl;
        cout << "Dispersion of empty slots: " << dispers << endl;
        cout << "Longest empty slot: " << maxwind << endl;
        cout << "Longest empty slot (in day): " << maxwindday << endl;
    }
    else
    {
        cout << "Not calc empty slots" << endl;
    }
    cout << endl;
}

void print_schedule_pupils(vector<Group> &g, Settings &stn)
{
    for (int q = 1; q <= stn.sessions; q++)
    {
        int left, right;
        left = right = 0;
        if (stn.sessions == 2)
        {
            if (q == 1) {left = 0; right = stn.maxlessons / 2;}
            if (q == 2) {left = stn.maxlessons / 2 - 1; right = stn.maxlessons;}
        }
        else
        {
            left = 0; right = stn.maxlessons;
        }

        for (int c = 0; c < stn.countclasses; c++)
        {
            if (g[c].session == q)
            {
                cout << g[c].name << ";";
            }
        }
        cout << endl;
        for (int d = 0; d < stn.days; d++)
        {
            cout << daynames[d] << endl;
            for (int l = left; l < right; l++)
            {
                for (int c = 0; c < stn.countclasses; c++)
                {
                    if (g[c].session == q)
                    {
                        cout << g[c].lesson[d][l]->name;
                        if (g[c].lesson[d][l]->name2 != "")
                        {
                            cout << "/" << g[c].lesson[d][l]->name2;
                        }
                        if (g[c].lesson[d][l]->classroom != 0)
                        {
                            cout << ":" << g[c].lesson[d][l]->classroom;
                        }
                        if (g[c].lesson[d][l]->classroom2 != 0)
                        {
                            if (g[c].lesson[d][l]->classroom != 0)
                            {
                                cout << "/";
                            }
                            else
                            {
                                cout << ":";
                            }
                            cout << g[c].lesson[d][l]->classroom2;
                        }
                        cout << ";";
                    }
                }
                cout << endl;
            }
        }
    }
    cout << endl;
}

void print_schedule_teachers(vector<Teacher> &t, Settings &stn)
{
    bool iswork[100] = {0};
    int cntworks = 0;
    int ti = 0;
    int p = 0;
    for (vector<Teacher>::iterator it = t.begin(); it != t.end(); ++it)
    {
        for (int i = 0; i < stn.days; i++)
        {
            if (iswork[ti]) break;
            for (int j = 0; j < stn.maxlessons; j++)
            {
                if (!it->availability[i][j])
                {
                    iswork[ti] = true;
                    break;
                }
            }
        }
        if (iswork[ti] && it->name != "")
        {
            cout << it->name << ";";
            cntworks++;
        }
        ti++;
        if (p == stn.countteachers) break;
        p++;
    }
    cout << endl;
    for (int day = 0; day < stn.days; day++)
    {
        for (int j = 0; j < cntworks; j++)
        {
            cout << daynames[day] << ";";
        }
        cout << endl;

        for (int j = 0; j < stn.maxlessons; j++)
        {
            p = 0;
            for (vector<Teacher>::iterator it = t.begin(); it != t.end(); ++it)
            {
                if (iswork[p])
                {
                    if (!it->availability[day][j])
                    {
                        cout << it->lesson[day][j]->groupname;
                        cout << ":" << it->lesson[day][j]->name;
                        if (it->lesson[day][j]->teacherid == p && it->lesson[day][j]->classroom != 0)
                        {
                            cout  << ":" << it->lesson[day][j]->classroom;
                        }
                        else if (it->lesson[day][j]->teacherid2 == p && it->lesson[day][j]->classroom2 != 0)
                        {
                            cout << ":" << it->lesson[day][j]->classroom2;
                        }
                        cout << ";";
                    }
                    else
                    {
                        cout << ";";
                    }
                }
                p++;
                if (p == stn.countteachers) break;
            }
            cout << endl;
        }
    }
    cout << endl;
}

void print_schedule_rooms(vector<Teacher> &t, vector<Classroom> &r, Settings &stn)
{
    bool iswork[100] = {0};
    int cntworks = 0;
    int ri = 0;
    for (vector<Classroom>::iterator it = r.begin(); it != r.end(); ++it)
    {
        for (int i = 0; i < stn.days; i++)
        {
            if (iswork[ri]) break;
            for (int j = 0; j < stn.maxlessons; j++)
            {
                if (!it->availability[i][j])
                {
                    iswork[ri] = true;
                    break;
                }
            }
        }
        if (iswork[ri] && ri != 0)
        {
            cout << ri << ";";
            cntworks++;
        }
        ri++;
    }
    if (cntworks != 0)
    {
        cout << endl;
        for (int day = 0; day < stn.days; day++)
        {
            for (int j = 0; j < cntworks; j++)
            {
                cout << daynames[day] << ";";
            }
            cout << endl;

            for (int j = 0; j < stn.maxlessons; j++)
            {
                ri = 0;
                for (vector<Classroom>::iterator it = r.begin(); it != r.end(); ++it)
                {
                    if (iswork[ri])
                    {
                        if (!it->availability[day][j])
                        {
                            cout << it->lesson[day][j]->groupname;
                            cout << ":" << it->lesson[day][j]->name << ":";
                            if (it->lesson[day][j]->classroom == ri)
                            {
                                cout << t[it->lesson[day][j]->teacherid].name;
                            }
                            else if (it->lesson[day][j]->classroom2 == ri)
                            {
                                cout << t[it->lesson[day][j]->teacherid2].name;
                            }
                            cout << ";";
                        }
                        else
                        {
                            cout << ";";
                        }
                    }
                    ri++;
                }
                cout << endl;
            }
        }
        cout << endl;
    }
    else
    {
        cout << "No rooms" << endl << endl;
    }
}

void print_schedule_gym(Gym &gym, Settings &stn)
{
    for (int day = 0; day < stn.days; day++)
    {
        cout << daynames[day] << ";";
    }

    cout << endl;

    for (int l = 0; l < stn.maxlessons; l++)
    {
        for (int d = 0; d < stn.days; d++)
        {
            cout << gym.lesson[d][l].group1name;
            if (gym.lesson[d][l].group2course != 0)
            {
                cout << "/" << gym.lesson[d][l].group2name;
            }
            cout << ";";
        }
        cout << endl;
    }
    cout << endl;
}

void print_needed_lessons(vector<Group> &g, vector<Teacher> &t, Settings &stn)
{
    bool showtitle = false;
    bool found = false;
    for (int i = 0; i < stn.countclasses; i++)
    {
        bool showname = false;
        for (int j = 0; j < g[i].countsubjects; j++)
        {
            if (g[i].subject[j].countlessons > 0)
            {
                found = true;
                if (!showtitle)
                {
                    showtitle = true;
                    cout << "Teacher;Subject;Unspent hours" << endl;
                }
                if (!showname)
                {
                    showname = true;
                    cout << g[i].name << endl;
                }
                cout << t[g[i].subject[j].teacherid].name << ";" << g[i].subject[j].name << ";"
                                                          << g[i].subject[j].countlessons << endl;
            }
        }
    }
    if (!found)
    {
        cout << "No such lessons" << endl;
    }
    cout << endl;
}

void print_full_info(vector<Group> &classes, vector<Teacher> &teachers, vector<Classroom> &rooms, Gym &gym,
                     Settings &stn, int step, int bugstep, int brokendays, int type)
{
    ofstream out;
    streambuf *coutbuf;
    coutbuf = cout.rdbuf();
    if (stn.tofile)
    {
        if (type == 0) out.open(stn.debug_fname, ofstream::app);
        else if (type == 1) out.open(stn.output_fname, ofstream::app);
        cout.rdbuf(out.rdbuf());
    }

    cout << "Schedule for pupils:" << endl;
    print_schedule_pupils(classes, stn);
    cout << "Not spent hours:" << endl;
    print_needed_lessons(classes, teachers, stn);
    cout << "Schedule for teachers:" << endl;
    print_schedule_teachers(teachers, stn);
    cout << "Schedule for rooms:" << endl;
    print_schedule_rooms(teachers, rooms, stn);
    cout << "Schedule for gym:" << endl;
    print_schedule_gym(gym, stn);
    cout << "Work hours:" << endl;
    print_teachers_winds(teachers, stn);
    cout << "Empty time slots statistic:" << endl;
    cout << "Total schedules: " << step + bugstep << endl;
    cout << "Right: " << step << endl;
    cout << "Wrong: " << bugstep << endl;
    cout << "Reset days: " << brokendays << endl;
    cout << "Ratio: " << 100.0 * step / (step + bugstep) << "% right" << endl;

    stn.end_t = clock();
    double elapsed_secs = double(stn.end_t - stn.begin_t) / CLOCKS_PER_SEC;

    cout << "Count time: " << (int)round(elapsed_secs) << " secs" << endl;
    cout << endl;

    if (stn.tofile)
    {
        cout.rdbuf(coutbuf);
    }
}
