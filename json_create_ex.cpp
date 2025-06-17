#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <filesystem>  // C++17 이상 필요
using json = nlohmann::json;

// // JSON 파일 생성 함수
// void createEmptyJsonFile(const std::string& name, const std::string& path = "./user_search/", bool asArray = false) {
//     std::filesystem::create_directories(path);  // 경로 없으면 생성

//     std::string filename = path + "/MENU_" + name + ".json";

//     std::ofstream file(filename);
//     if (!file.is_open()) {
//         std::cerr << "파일을 생성할 수 없습니다: " << filename << std::endl;
//         return;
//     }

//     json empty = asArray ? json::array() : json::object();  // [] 또는 {}

//     file << empty.dump(4);  // 들여쓰기 포함 저장
//     file.close();

//     std::cout << filename << " 파일이 생성되었습니다." << std::endl;
// }

// // 메인 예제
// int main() {
//     // 예제 1: 기본 경로에 빈 JSON 객체 생성
//     createEmptyJsonFile("001");

//     // 예제 2: ./data 폴더에 빈 JSON 배열 생성
//     createEmptyJsonFile("today_menu", "./data", true);

//     // 예제 3: ./config 폴더에 빈 객체 생성
//     createEmptyJsonFile("settings", "./config");

//     // 예제 4: 절대 경로 사용 예 (환경에 맞게 경로 조정)
//     createEmptyJsonFile("log", "/tmp/json_logs");

//     return 0;
// }

// 현재 시간 반환
std::string getCurrentTimeString() {
    std::time_t now = std::time(nullptr);
    char buf[100];
    std::strftime(buf, sizeof(buf), "%F %T", std::localtime(&now));
    return std::string(buf);
}

// 메뉴 항목 추가 및 삭제 (최신순 저장 포함)
void appendMenuItemLatestFirst(const std::string& name, const std::string& path, const json& newItem) {
    std::filesystem::create_directories(path);
    std::string filename = path + "/MENU_" + name + ".json";

    json data = json::array();  // 기본 빈 배열

    // 파일이 존재하면 열기
    if (std::filesystem::exists(filename)) {
        std::ifstream inFile(filename);
        if (inFile.is_open()) {
            try {
                inFile >> data;
                if (!data.is_array()) {
                    std::cerr << "파일 형식이 배열이 아닙니다: " << filename << std::endl;
                    data = json::array();  // 강제 초기화
                }
            } catch (...) {
                std::cerr << "기존 JSON 파싱 오류: " << filename << std::endl;
                data = json::array();  // 파싱 실패 시 초기화
            }
            inFile.close();
        }
    }

    // 20개 초과 시 마지막 항목 제거
    if (data.size() >= 20) {
        data.erase(data.end() - 1);  // 가장 오래된 항목 제거
    }

    // 최신 항목을 맨 앞에 추가
    json itemToAdd = newItem;
    itemToAdd["timestamp"] = getCurrentTimeString();  // 타임스탬프 추가
    data.insert(data.begin(), itemToAdd);

    // 저장
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        std::cerr << "파일을 저장할 수 없습니다: " << filename << std::endl;
        return;
    }

    outFile << data.dump(4);
    outFile.close();

    std::cout << filename << "에 항목이 추가되었으며 ";
    if (data.size() == 20) std::cout << "가장 오래된 항목이 제거되었습니다.";
    std::cout << std::endl;
}

// // 예제 main
// int main() {
//     std::string name = "log";
//     std::string path = "./data";

//     json newMenuItem = {
//         {"menu_id", 101},
//         {"name", "불고기버거"},
//         {"price", 5800},
//         {"timestamp", getCurrentTimeString()}
//     };

//     appendMenuItemLatestFirst(name, path, newMenuItem);

//     return 0;
// }

int main() {
    std::string name = "log";
    std::string path = "./data";

    json newMenuItem = {
        {"menu_id", 102},
        {"name", "고구마피자"},
        {"price", 9800}
    };

    // 1. 항목 추가만
    appendMenuItemLatestFirst(name, path, newMenuItem);

    // 2. 항목 추가 + menu_id == 101 항목 삭제
    json anotherItem = {
        {"menu_id", 103},
        {"name", "치즈스틱"},
        {"price", 3200}
    };

    return 0;
}