#ifndef CSVROW_H
#define CSVROW_H

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

class CSVRow
{
public:
    CSVRow(void);
    string const& operator[] (size_t index) const;
    size_t size() const;
    void readNextRow(istream& str);

private:
    vector<string> m_data;
    string default_val;
};

istream& operator>>(istream& str, CSVRow& data);

#endif // CSVROW_H
