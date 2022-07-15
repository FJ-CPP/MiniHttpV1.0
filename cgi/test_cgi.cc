#include "Common.hpp"
using namespace std;

int main()
{
    string method = getenv("METHOD");
    
    string query;
    GetQuery(method, query);
    // query的格式为：x=?&y=?
    string q1;
    string q2;
    CutString(query, "&", q1, q2);

    string name1, value1;
    string name2, value2;
    CutString(q1, "=", name1, value1);
    CutString(q2, "=", name2, value2);

    // 本进程的1号fd已经重定向为管道的写端
    cerr << name1 << " : " << value1 << endl;
    cerr << name2 << " : " << value2 << endl;

    int x = stoi(value1);
    int y = stoi(value2);

    // 简单计算器 +-*/
    cout << "<!DOCTYPE html>";
    cout << "<html>";
    cout << "<meta charset=\"utf-8\">";
    cout << "<head>";
    cout << "<p3>" << x << "+" << y << " = " << x + y << "</p3>" << "<br>";
    cout << "<p3>" << x << "-" << y << " = " << x - y << "</p3>" << "<br>";
    cout << "<p3>" << x << "*" << y << " = " << x * y << "</p3>" << "<br>";
    if (y != 0)
    {
        cout << "<p3>" << x << "/" << y << " = " << x / y << "</p3>" << "<br>";
    }
    else
    {
        cout << "<p3>" << "除数不得为零" << "</p3>" << "<br>";
    }
    cout << "</head>";
    cout << "</html>";
    return 0;
}
