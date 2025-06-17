#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>
#include <mutex>
#include <string>
#include <netinet/in.h>    // 소켓 프로그래밍
#include <unistd.h>        // close(), read(), write()
#include <mariadb/mysql.h> // MariaDB 연결

using json = nlohmann::json;
using namespace std;
std::mutex cout_mutex;


MYSQL* connect_db() {
    MYSQL* conn = mysql_init(nullptr);
    if (!mysql_real_connect(conn, "10.10.20.110", "LBH", "1234", "CouCouEats", 0, NULL, 0)) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "MariaDB connection failed: " << mysql_error(conn) << std::endl;
        return nullptr;
    }
    return conn;
}

void createEmptyJsonFile(const std::string& name, bool asArray = false) {
    std::string filename = "MENU_" + name + ".json";

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "파일을 생성할 수 없습니다: " << filename << std::endl;
        return;
    }

    json empty = asArray ? json::array() : json::object();  // {} 또는 []

    file << empty.dump(4);  // 들여쓰기 4칸
    file.close();

    std::cout << filename << " 빈 JSON " 
              << (asArray ? "배열" : "객체") 
              << " 파일이 생성되었습니다." << std::endl;
}


int main() {
    const std::string filename = "menu_item_001.json";
    json j;

    // 1. JSON 파일 열기
    std::ifstream input_file(filename);
    if (!input_file.is_open()) {
        std::cerr << "Failed to open " << filename << std::endl;
        return 1;
    }

    try {
        input_file >> j;  // 파일 내용을 JSON으로 파싱
        input_file.close();
    } catch (const json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return 1;
    }

    cout << j;
    // // cout << "option:" << j["options"][0]["option_category"];
    // if (j["options"][0]["option_category"]=="음료선택")
    // {
    //     cout << "option:" << j["options"][0]["options"];
    // }
    

    // MYSQL* conn = connect_db();
    // std::string query = "INSERT INTO STORE(STORE_NAME, STORE_ADDR, PHONE_NUM, OWNER_NUM, COMPANY_NUM, OPENING_TIME,\
    // CLOSING_TIME, DELIVERY_FEE, MINIMUM_ORDER, STORE_DESC, MENU_INFO, OPTION_INFO, ORIGIN_INFO, NUTRITION_INFO, ALLERGEN_INFO)";

    // mysql_query(conn, query.c_str());
    return 0;
}