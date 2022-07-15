#include <iostream>
#include "include/mysql.h"
#include "Common.hpp"

using namespace std;

bool InsertSql(string sql)
{
    MYSQL *conn = mysql_init(nullptr);
    mysql_set_character_set(conn, "utf8");
    if (mysql_real_connect(conn, "127.0.0.1", "http_server", "12345678", "http_test", 3306, nullptr, 0) == nullptr)
    {
        cerr << "connect error!" << endl;
        return false;
    }
    cerr << "connect to mysql success!" << endl;

    int ret = mysql_query(conn, sql.c_str());
    cerr << "sql result: " << ret << endl;
    mysql_close(conn);
    return true;
}

int main()
{
    string method = getenv("METHOD");
    
    string query;
    GetQuery(method, query);
    cerr << "query : " << query << endl;
    // query的格式为：name=?&password=?
    string q1;
    string q2;
    CutString(query, "&", q1, q2);
    cerr << "q1" << " : " << q1 << endl;
    cerr << "q2" << " : " << q2 << endl;

    string name, _name;
    string password, _password;
    CutString(q1, "=", name, _name);
    CutString(q2, "=", password, _password);
    
    string sql = "insert into user (name, password) values (\'";
    sql += _name + "\', \'";
    sql += _password + "\'";
    sql += ")";
    
    InsertSql(sql);
    // 注册页面
    cout << "<!DOCTYPE html>";
    cout << "<html>";
    cout << "<meta charset=\"utf-8\">";
    cout << "<head>";
    cout << "<p1>用户 "+ _name + " 注册成功！</p1>";
    cout << "</head>";
    cout << "</html>";
    return 0;
}
