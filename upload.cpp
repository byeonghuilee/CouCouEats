// 표준 및 외부 라이브러리 헤더
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <string>
#include <netinet/in.h>    // 소켓 프로그래밍
#include <unistd.h>        // close(), read(), write()
#include <nlohmann/json.hpp> // JSON 파싱
#include <mariadb/mysql.h> // MariaDB 연결

using json = nlohmann::json;
using namespace std;

// 출력 시 동시 접근 방지를 위한 전역 뮤텍스
std::mutex cout_mutex;

// 서버가 리슨할 포트 정의
constexpr int PORT = 10001;

// 클라이언트에 JSON 응답 전송 함수
void send_json(int client_sock, const json& response) {
    std::string output = response.dump();
    send(client_sock, output.c_str(), output.size(), 0);
}

//////////////////////////////
// MariaDB 연결 함수 정의
//////////////////////////////
MYSQL* connect_db() {
    MYSQL* conn = mysql_init(nullptr);
    if (!mysql_real_connect(conn, "10.10.20.110", "LBH", "1234", "CouCouEats", 0, NULL, 0)) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "MariaDB connection failed: " << mysql_error(conn) << std::endl;
        return nullptr;
    }
    return conn;
}


int main() {
    MYSQL* conn = connect_db();
    std::string query = "DELETE FROM users WHERE id=" + to_string(id);
    mysql_query(conn, query.c_str());
    return 0;
}