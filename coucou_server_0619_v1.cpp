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
#include <unistd.h> // send
#include <fstream>
#include <map>
#include <arpa/inet.h>
#include <random>
#include <ctime>
#include <iomanip>
#include <sstream>




using json = nlohmann::json;
using namespace std;
void createEmptyJsonFile(const std::string& name);
void createEmptyJsonFile_new(const std::string& name, const std::string& path, bool asArray);
void appendMenuItemLatestFirst(const std::string& name, const std::string& path, const json& newItem);
string getCurrentTimeString();
string category_name(int category);
int get_socket_for_ip(const std::string& ip);
string generate_random_code(int length);
string getCurrentTimestamp();
string getCurrentDateOnly();

// 출력 시 동시 접근 방지를 위한 전역 뮤텍스
std::mutex cout_mutex;
std::string buffer;  // 누적 버퍼

// std::mutex ip_map_mutex;
// std::map<std::string, int> ip_to_socket;
std::map<std::string, int> ip_to_socket;
std::mutex ip_map_mutex;


struct socket_info {
    std::string ip;
    int sock;
};

// 서버가 리슨할 포트 정의
constexpr int PORT = 20019;


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
    if (!mysql_real_connect(conn, "10.10.20.118", "LBH", "1234", "CouCouEats", 0, NULL, 0)) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "MariaDB connection failed: " << mysql_error(conn) << std::endl;
        return nullptr;
    }
    return conn;
}

/////////////////////////////////////
// JSON 메시지 처리 (CRUD 핸들러)
/////////////////////////////////////
void handle_json_message(const std::string& message, int client_sock) {
    // cout << "메세지1:" << client_sock;    
    try {
        auto j = json::parse(message);
        if (!j.contains("signal")) {
            send_json(client_sock, { {"status", "error"}, {"message", "'signal' field missing"}});
            return;
        }
        
        // socket_info s_info;
        // std::string who = j["who"];
        // s_info.id.append(who);
        // s_info.socket.append(to_string(client_sock));

        std::string signal = j["signal"];
        MYSQL* conn = connect_db();
        if (!conn) return;

        if (signal == "create") 
        {
            json res;

            string messages = res.dump();
            send(client_sock, messages.c_str(), messages.size(), 0);
            // std::string name = j["name"];
            // int age = j["age"];
            // std::string query = "INSERT INTO users (name, age) VALUES ('" + name + "', " + to_string(age) + ")";
            // if (mysql_query(conn, query.c_str()) == 0) {
            //     send_json(client_sock, { {"status", "success"}, {"message", "User created"} });
            // } else {
            //     send_json(client_sock, { {"status", "error"}, {"message", "잘가요!"} });
            // }

        } 
        else if (signal == "search")    // 로그인 시 가게 정보 전송
        {
            std::string query = "SELECT * FROM STORE";
            string ca_name;

            if (mysql_query(conn, query.c_str()) == 0) {
                MYSQL_RES* result = mysql_store_result(conn);
                MYSQL_ROW row;

                std::map<std::string, json> store_by_category;

                while ((row = mysql_fetch_row(result))) {
                    const char* origin_info_ptr = row[13];
                    std::string filename = origin_info_ptr ? origin_info_ptr : "";

                    json j;
                    if (!filename.empty()) {
                        std::ifstream input_file(filename);
                        try {
                            input_file >> j;
                            input_file.close();
                        } catch (const json::parse_error& e) {
                            std::cerr << "JSON parse error: " << e.what() << std::endl;
                            continue;  // 이 항목 건너뛰기
                        }
                    }

                    int category = 0;
                    if (row[1]) {
                        try {
                            category = std::stoi(row[1]);
                        } catch (const std::exception& e) {
                            category = 0;
                        }
                    }

                    ca_name = category_name(category);  // 예: 1 -> "한식"

                    json store_info = {
                        {"category_num", category},
                        {"store_name", row[2] ? row[2] : ""},
                        {"store_addr", row[3] ? row[3] : ""},
                        {"phone_num", row[4] ? row[4] : ""},
                        {"owner_name", row[5] ? row[5] : ""},
                        {"company_number", row[6] ? row[6] : ""},
                        {"opening_time", row[7] ? row[7] : ""},
                        {"closing_time", row[8] ? row[8] : ""},
                        {"delivery_fee", row[10] ? row[10] : ""},
                        {"minimum_order", row[11] ? row[11] : ""},
                        {"store_desc", row[12] ? row[12] : ""},
                        {"menu_info", j},
                        {"origin_info", row[14] ? row[14] : ""},
                        {"nutrition_fact", row[15] ? row[15] : ""},
                        {"allergen_info", row[16] ? row[16] : ""},
                        {"stataus", row[17] ? row[17] : ""}
                    };

                    // 카테고리 이름별로 store 정보 누적
                    store_by_category[ca_name].push_back(store_info);
                }

                mysql_free_result(result);

                // 최종 total 배열 생성
                json total = json::array();
                for (const auto& [category_name, store_list] : store_by_category) {
                    total.push_back({
                        {"category_name", category_name},
                        {"store_info", store_list}
                    });
                }

                // std::cout << "store_info::" << total << std::endl;
                send_json(client_sock, { {"signal", "main_data"}, {"store_info", total} });

            } else {
                send_json(client_sock, { {"status", "error"}, {"message", mysql_error(conn)} });
            }
        }
        else if (signal == "update")    // 회원 정보 수정
        {
            int id = j["id"];
            std::string name = j["name"];
            int age = j["age"];
            std::string query = "UPDATE users SET name='" + name + "', age=" + to_string(age) + " WHERE id=" + to_string(id);
            if (mysql_query(conn, query.c_str()) == 0) {
                send_json(client_sock, { {"status", "success"}, {"message", "User updated"} });
            } else {
                send_json(client_sock, { {"status", "error"}, {"message", mysql_error(conn)} });
            }

        }
        else if (signal == "delete")    // 삭제
        {
            int id = j["id"];
            std::string query = "DELETE FROM users WHERE id=" + to_string(id);
            if (mysql_query(conn, query.c_str()) == 0) {
                send_json(client_sock, { {"status", "success"}, {"message", "User deleted"} });
            } else {
                send_json(client_sock, { {"status", "error"}, {"message", mysql_error(conn)} });
            }

        }
        else if (signal == "sign_up_result")    // 회원가입 결과
        {
            string role = j["role"];
            string p_num = j["pnum"];
            if (role == "user")
            {
                string query = "SELECT * FROM USER_INFO WHERE PNUM = " + p_num;

                if (mysql_query(conn, query.c_str()) == 0)
                {
                    MYSQL_RES* result = mysql_store_result(conn);  // 결과 저장
                    if (result != nullptr)
                    {
                        int num_rows = mysql_num_rows(result);  // 결과 행 개수 확인
                        if (num_rows > 0) {
                            send_json(client_sock, {{"signal", "sign_up_result"},{"result", "fail"}});
                        } else {
                            send_json(client_sock, {{"signal", "sign_up_result"},{"result", "success"}});
                        }
                        mysql_free_result(result);  // 메모리 해제
                    }
                    else {
                        // SELECT는 성공했지만 결과가 없음 또는 오류 발생
                        send_json(client_sock, {{"result", "error"}});
                    }
                }
                else {
                    // 쿼리 실행 실패
                    send_json(client_sock, {{"result", "query fail"}});
                }
            }
            else if (role == "store")
            {
                cout << "sign_up_result:" << j << endl;
                string business_id = j[""];
                string query = "SELECT * FROM OWNER_INFO WHERE BUSINESS_ID = " + business_id;

                if (mysql_query(conn, query.c_str()) == 0)
                {
                    MYSQL_RES* result = mysql_store_result(conn);  // 결과 저장
                    if (result != nullptr)
                    {
                        int num_rows = mysql_num_rows(result);  // 결과 행 개수 확인
                        if (num_rows > 0) {
                            send_json(client_sock, {{"signal", "sign_up_result"},{"result", "fail"}});
                        } else {
                            send_json(client_sock, {{"signal", "sign_up_result"},{"result", "success"}});
                        }
                        mysql_free_result(result);  // 메모리 해제
                    }
                    else {
                        // SELECT는 성공했지만 결과가 없음 또는 오류 발생
                        send_json(client_sock, {{"result", "error"}});
                    }
                }
                else {
                    // 쿼리 실행 실패
                    send_json(client_sock, {{"result", "query fail"}});
                }
            }
            else if (role == "rider")
            {
                string business_id = j[""];
                string query = "SELECT * FROM OWNER_INFO WHERE BUSINESS_ID = " + business_id;

                if (mysql_query(conn, query.c_str()) == 0)
                {
                    MYSQL_RES* result = mysql_store_result(conn);  // 결과 저장
                    if (result != nullptr)
                    {
                        int num_rows = mysql_num_rows(result);  // 결과 행 개수 확인
                        if (num_rows > 0) {
                            send_json(client_sock, {{"signal", "sign_up_result"},{"result", "fail"}});
                        } else {
                            send_json(client_sock, {{"signal", "sign_up_result"},{"result", "success"}});
                        }
                        mysql_free_result(result);  // 메모리 해제
                    }
                    else {
                        // SELECT는 성공했지만 결과가 없음 또는 오류 발생
                        send_json(client_sock, {{"result", "error"}});
                    }
                }
                else {
                    // 쿼리 실행 실패
                    send_json(client_sock, {{"result", "query fail"}});
                }
            }
            


            
            // int size_ = query.size();

            // for (int i = 0; i < size_; i++)
            // {
            //     query[i] = p_num;   
            // }
            
                    
            // if (mysql_query(conn, query.c_str())==0)
            // {
            //     send_json(client_sock, {"signal", "success"});
            // }
            // else {
            //     send_json(client_sock, {"signal", "fail"});
            // }
            
        }
        else if (signal == "sign_up")   // 회원 가입
        {  
            string role = j["role"];
            if (role == "user")
            {
                string email = j["e_mail"];
                string pw = j["pw"];
                string name = j["name"];
                string pnum = j["pnum"];
                string birth = j["birth"];
                string query = "INSERT INTO USER_INFO(EMAIL, PW, NAME, PNUM, BIRTH) VALUES('"+email+"','"+pw+"','"+name+"','"+pnum+"','"+birth+"')";
                
          
                if (mysql_query(conn, query.c_str()) == 0) {
                    // send_json(client_sock, { {"status", "sign_up"}, {"result", "회원가입 완료!"}, {"user_id", user_id}, {"e-mail", u_email}});
                    cout << "회원가입 완료" << endl;
                } else {
                    // send_json(client_sock, { {"status", "error"}, {"result", "회원가입에 실패 하셨습니다."} });
                    cout << "회원가입 실패" << endl;
                }
             
                
            } 
            else if (role == "store")
            {

                string BRNUM = j["business_number"];
                string BUSINESS_ID = j["id"];
                string PW = j["pass"];
                string NAME = j["owner_name"];
                string PNUM = j["owner_phone"];
                string INDUSTRY = "일반음식점";
                // 영업점 정보 ================================
                string STORE_NAME = j["store_name"];
                string STORE_ADDR = j["store_addr"];
                string PHONE_NUM = j["store_phone"];
                string OWNER_NAME = j["owner_name"];
                string COMPANY_NUM = j["business_number"];
                string STORE_DESC = j["store_intro"];
                string OPENING_TIME = j["open_time"];
                string CLOSING_TIME = j["close_time"];
                string MINIMUM_ORDER = j["min_order_le"];
                string DELIVERY_AREA = j["deliver_areas"];
                string ORIGIN_INFO = j["origin_info"];
                string NUTRITION_FACT = j["nutritional_info"];
                string ALLERGEN_INFO = j["allergy_info"];

               
                createEmptyJsonFile(STORE_NAME);
               
        
                string query = "INSERT INTO OWNER_INFO(BRNUM, BUSINESS_ID, PW, NAME, PNUM, INDUSTRY) VALUES('"+BRNUM+"','"+BUSINESS_ID+"','"+PW+"','"+NAME+"','"+PNUM+"','"+INDUSTRY+"')";
                string query2 = "INSERT INTO STORE(STORE_NAME, STORE_ADDR, PHONE_NUM,OWNER_NAME,COMPANY_NUM,STORE_DESC,OPENING_TIME,CLOSING_TIME,MINIMUM_ORDER,DELIVERY_AREA) VALUES('"+STORE_NAME+"','"+STORE_ADDR+"','"+PHONE_NUM+"','"+OWNER_NAME+"','"+COMPANY_NUM+"','"+STORE_DESC+"','"+OPENING_TIME+"','"+CLOSING_TIME+"','"+MINIMUM_ORDER+"','"+DELIVERY_AREA+"')";
                if (mysql_query(conn, query.c_str()) == 0 && mysql_query(conn, query2.c_str())==0) {
                    send_json(client_sock, { {"status", "sign_up"}, {"result", "회원가입 완료!"} });
                    cout << "회원가입 완료" << endl;
                } else {
                    send_json(client_sock, { {"status", "error"}, {"result", "회원가입에 실패 하셨습니다."} });
                    cout << "회원가입 실패" << endl;
                }
            }   
        }
        else if (signal == "login") // 로그인(USER & STORE)
        {
            string role = j["role"];
            
            json user_cart = json::array();

            if (role == "user") {
                string email = j["e_mail"];
                string pw = j["pw"];
                json user_cart = json::array();

                // 1. 로그인 확인
                string login_query = "SELECT * FROM USER_INFO WHERE EMAIL = '" + email + "' AND PW = '" + pw + "'";

                if (mysql_query(conn, login_query.c_str()) == 0) {
                    MYSQL_RES* result = mysql_store_result(conn);

                    // 결과 없음 또는 비어있는 경우
                    if (!result || mysql_num_rows(result) == 0) {
                        send_json(client_sock, {{"signal", "login_result"}, {"result", "fail"}});
                        if (result) mysql_free_result(result);
                        return;
                    }

                    MYSQL_ROW row = mysql_fetch_row(result);

                    // 로그인 성공 → 사용자 정보 추출
                    // string user_id = row[0] ? row[0] : "";
                    string u_email = row[1] ? row[1] : "";
                    string name    = row[3] ? row[3] : "";
                    string pnum    = row[4] ? row[4] : "";

                    mysql_free_result(result);  // 사용자 쿼리 결과 해제

                    // 2. 장바구니 정보 조회
                    string cart_query =
                        "SELECT c.CART_ID, c.USER_EMAIL, c.STORE_NAME, c.TOTAL_PRICE, c.STATUS, "
                        "d.CD_ID, d.MENU, d.OPTION, d.PRICE, d.QUANTITY "
                        "FROM CARTS c JOIN CART_DETAILS d ON c.CART_ID = d.CART_ID "
                        "WHERE c.USER_EMAIL = '" + email + "'";

                    if (mysql_query(conn, cart_query.c_str()) == 0) {
                        MYSQL_RES* cart_result = mysql_store_result(conn);
                        MYSQL_ROW cart_row;
                        int cart_id = 0;
                        json cart_detail = json::array();
                        while ((cart_row = mysql_fetch_row(cart_result)) != nullptr) {
                            
                            // try {
                            //     cart_id = cart_row[0] ? std::stoi(cart_row[0]) : 0;
                            // } catch (const std::exception& e) {
                            //     cart_id = 0;
                            // }
                            cart_detail.push_back({
                                {"MENU", cart_row[6] ? cart_row[6] : ""},
                                {"OPTION", cart_row[7] ? cart_row[7] : ""},
                                {"PRICE", cart_row[8] ? cart_row[8] : ""},
                                {"QUANTITY", cart_row[9] ? cart_row[9] : ""}
                            });
                        }
                        
                        string carts_query = "SELECT * FROM CARTS WHERE USER_EMAIL = '"+email+"'";
                        mysql_query(conn, carts_query.c_str());
                        MYSQL_RES* carts_result = mysql_store_result(conn);
                        MYSQL_ROW carts_row = mysql_fetch_row(carts_result);
                        json cart_item = {
                            {"store_name", carts_row[2] ? carts_row[2] : ""},
                            {"total_price", carts_row[3] ? carts_row[3] : ""},
                            {"eat_type", carts_row[4] ? carts_row[4] : ""},
                            {"cart_items", cart_detail}                               
                        };

                        user_cart.push_back(cart_item);
                        mysql_free_result(cart_result);
                        mysql_free_result(carts_result);
                    } else {
                        std::cerr << "장바구니 쿼리 실패: " << mysql_error(conn) << std::endl;
                    }

                    json response = {
                        {"signal", "login"},
                        {"result", "success"},
                        {"email", u_email},
                        {"name", name},
                        {"pnum", pnum}
                    };

                    if (!user_cart.empty()) {
                        response["cart"] = user_cart;
                    }

                    // 4. 주소 정보
                    string add_query = "SELECT * FROM USER_ADDR WHERE EMAIL = '"+email+"'";

                    if (mysql_query(conn, add_query.c_str())==0)
                    {
                        MYSQL_RES* result = mysql_store_result(conn);
                        MYSQL_ROW row;
                        json addr = json::array();
                        while ((row = mysql_fetch_row(result))) {
                            addr.push_back({
                                {"addr_id", row[0] ? to_string(std::stoi(row[0])) : ""},
                                {"email", row[1] ? row[1] : ""},
                                {"basic_addr", row[2] ? row[2] : ""},
                                {"detail_addr", row[3] ? row[3] : ""},
                                {"direction", row[4] ? row[4] : ""},
                                {"is_primary", row[5] ? row[5] : ""},
                                {"type", row[6] ? row[6] : ""}
                            });
                            if (row == nullptr)
                            {
                                response["user_add"] = "no";
                            }
                        }

                        mysql_free_result(result);
                        response["user_add"] = addr;
                        

                        
                    }
                    // 5. 검색어 정보
                    // string result_query = "SELECT * FROM KEYWORD WHERE USER_ID= '"+email+"'";

                    // if (mysql_query(conn, result_query.c_str()) == 0) {
                    //     MYSQL_RES* result = mysql_store_result(conn);
                    //     MYSQL_ROW row;
                    //     json keyword = json::array();

                    //     while ((row = mysql_fetch_row(result))) {
                    //         const char* user_keyword = row[2];  // 파일 이름 저장된 컬럼
                    //         std::string filename = user_keyword ? user_keyword : "";

                    //         json k;
                    //         if (!filename.empty()) {
                    //             std::ifstream input_file(filename);
                    //             try {
                    //                 input_file >> k;  // 파일 내용을 JSON으로 파싱
                    //                 input_file.close();
                    //             } catch (const json::parse_error& e) {
                    //                 std::cerr << "JSON parse error: " << e.what() << std::endl;
                    //                 return;
                    //             }
                                
                    //         }
                    //         else
                    //         {
                    //             keyword = {};
                    //             break;
                    //         }
                    //         keyword.push_back({"keyword", k});
                    //     }
                    //     mysql_free_result(result);
                    //     response["keyword"] = keyword;
                    // }
                    // 6. 리뷰 정보

                    // 7. 주문 내역

                    // 8. 즐겨 찾기

                    send_json(client_sock, {{"signal", "login_result"}, {"result", "success"},{"user_info",response}});
                }
                else {
                    // send_json(client_sock, {{"signal", "login_result"}, {"result", "query fail"}});
                }
            }

            else if (role == "store")
            {
                cout << "store : " << j << endl;
                string store_id = j["id"];
                string pw = j["pw"];
                string business_num = j["biz_num"];
                string login_query = "SELECT * FROM OWNER_INFO WHERE BUSINESS_ID = '" + store_id + "' AND PW = '" + pw + "'";

                if (mysql_query(conn, login_query.c_str()) == 0)
                {
                    MYSQL_RES* result = mysql_store_result(conn);  // 결과 저장

                    // 결과 없음 또는 실패 처리
                    if (!result || mysql_num_rows(result) == 0) {
                        send_json(client_sock, {
                            {"signal", "response_store_login"},
                            {"store_login_result", "fail"},
                            {"message", "사업자 정보를 찾을 수 없습니다."}
                        });
                        if (result) mysql_free_result(result);
                        return;
                    }

                    MYSQL_ROW row = mysql_fetch_row(result);
                    std::string pnum;
                    string company_num;

                    if (row && row[1] && row[5]) {
                        pnum = row[5];
                        company_num = row[1];
                        cout << "pnum : " << pnum << endl;
                        cout << "company_num : " << company_num << endl;
                    } else {
                        send_json(client_sock, {
                            {"signal", "response_store_login"},
                            {"store_login_result", "fail"},
                            {"message", "로그인 정보가 누락되어 있습니다."}
                        });
                        mysql_free_result(result);
                        return;
                    }

                    mysql_free_result(result);  // 로그인 결과 해제

                    // STORE 테이블에서 정보 조회
                    std::string query2 = "SELECT * FROM STORE WHERE COMPANY_NUM= '" + company_num + "'";

                    if (mysql_query(conn, query2.c_str()) == 0) {
                        MYSQL_RES* result = mysql_store_result(conn);
                        if (!result || mysql_num_rows(result) == 0) {
                            send_json(client_sock, {
                                {"signal", "response_store_login"},
                                {"store_login_result", "fail"},
                                {"message", "매장 정보를 찾을 수 없습니다."}
                            });
                            if (result) mysql_free_result(result);
                            return;
                        }

                        MYSQL_ROW row;
                        json store = json::array();

                        while ((row = mysql_fetch_row(result))) {
                            const char* origin_info_ptr = row[13];  // 메뉴파일 경로
                            std::string filename = origin_info_ptr ? origin_info_ptr : "";

                            json j;
                            if (!filename.empty()) {
                                std::ifstream input_file(filename);
                                try {
                                    input_file >> j;
                                    input_file.close();
                                } catch (const json::parse_error& e) {
                                    std::cerr << "JSON parse error: " << e.what() << std::endl;
                                    j = json::array();  // 오류 시 빈 배열로 대체
                                }
                            }

                            int category = 0;
                            if (row[1]) {
                                try {
                                    category = std::stoi(row[1]);
                                } catch (const std::invalid_argument& e) {
                                    std::cerr << "변환할 수 없는 문자열입니다: " << row[1] << " (" << e.what() << ")\n";
                                    category = 0;
                                } catch (const std::out_of_range& e) {
                                    std::cerr << "숫자가 너무 큽니다: " << row[1] << " (" << e.what() << ")\n";
                                    category = 0;
                                }
                            }

                            store.push_back({
                                {"category_num", category},
                                {"store_name", row[2] ? row[2] : ""},
                                {"store_addr", row[3] ? row[3] : ""},
                                {"phone_num", row[4] ? row[4] : ""},
                                {"owner_name", row[5] ? row[5] : ""},
                                {"company_num", row[6] ? row[6] : ""},
                                {"opening_time", row[7] ? row[7] : ""},
                                {"closing_time", row[8] ? row[8] : ""},
                                {"delivery_fee", row[10] ? row[10] : ""},
                                {"minimum_order", row[11] ? row[11] : ""},
                                {"store_desc", row[12] ? row[12] : ""},
                                {"menu_info", j},
                                {"origin_info", row[14] ? row[14] : ""},
                                {"nutrition_fact", row[15] ? row[15] : ""},
                                {"allergen_info", row[16] ? row[16] : ""},
                                {"stataus", row[17] ? row[17] : ""}
                            });
                        }

                        mysql_free_result(result);

                        // ORDER_INFO 테이블에서 해당 가게 정보 조회
                        string order_query = "SELECT * FROM ORDER_INFO WHERE BUSINESS_NUM= '"+business_num+"'";
                        json order_info = json::array();
                        if (mysql_query(conn, order_query.c_str())==0)
                        {
                            MYSQL_RES* order_result = mysql_store_result(conn);
                            MYSQL_ROW row;
                            while ((row=mysql_fetch_row(order_result)))
                            {
                                order_info.push_back({
                                    {"order_serialNumber", row[1] ? row[1] : ""},
                                    {"order_time", row[13] ? row[13] : ""},
                                    {"to_owner", row[10] ? row[10] : ""},
                                    {"to_rider", row[11] ? row[11] : ""},
                                    {"once_plastic", row[9] ? row[9] : ""},
                                    {"user_pnum", row[3] ? row[3] : ""},
                                    {"user_addr", row[4] ? row[4] : ""},
                                    {"order_status", row[16] ? row[16] : ""},
                                    {"store_name", row[5] ? row[5] : ""},
                                    {"total_price", row[7] ? row[7] : ""},
                                    {"eat_type", row[8] ? row[8] : ""},
                                    {"order_items", row[12] ? row[12] : ""}
                                });
                            }
                            mysql_free_result(order_result);
                        }
                        
                        send_json(client_sock, {
                            {"signal", "response_store_login"},
                            {"store_login_result", "success"},
                            {"store_info", store},
                            {"order_info", order_info}
                        });
                    }
                    else {
                        send_json(client_sock, {
                            {"signal", "response_store_login"},
                            {"store_login_result", "fail"},
                            {"message", mysql_error(conn)}
                        });
                    }
                }
                else {
                    send_json(client_sock, {
                        {"signal", "response_store_login"},
                        {"store_login_result", "fail"},
                        {"message", mysql_error(conn)}
                    });
                }
            }   
        }
        else if (signal == "find_id")   // User ID 찾기
        {
            string role = j["role"];
            string name = j["name"];
            string pnum = j["pnum"];
            if(role=="user") {
                auto escape_quotes = [](const std::string& input) {
                    std::string escaped;
                    for (char c : input) {
                        if (c == '\'') escaped += "\\'";
                        else escaped += c;
                    }
                    return escaped;
                };

                std::string safe_name = escape_quotes(name);
                std::string safe_pnum = escape_quotes(pnum);

                std::string query = "SELECT EMAIL FROM USER_INFO WHERE NAME = '" + safe_name + "' AND PNUM = '" + safe_pnum + "'";

                if (mysql_query(conn, query.c_str()) == 0)
                {
                    MYSQL_RES* result = mysql_store_result(conn);  // 결과 저장
                    MYSQL_ROW row = mysql_fetch_row(result);
                    std::string email;

                    if (row && row[0]) {
                        email = row[0];
                    } else {
                        std::cout << "결과 없음 또는 NULL\n";
                    }
                    if (result != nullptr)
                    {
                        int num_rows = mysql_num_rows(result);  // 결과 행 개수 확인
                        cout << num_rows;
                        if (num_rows > 0) {
                            send_json(client_sock, {{"signal", "find_id_result"},{"result", "success"}, {"user_id",email}});
                        } else {
                            send_json(client_sock, {{"signal", "find_id_result"},{"result", "fail"}});
                        }
                        mysql_free_result(result);  // 메모리 해제
                    }
                    else {
                        // SELECT는 성공했지만 결과가 없음 또는 오류 발생
                        send_json(client_sock, {{"result", "error"}});
                    }
                }
                else {
                    // 쿼리 실행 실패
                    send_json(client_sock, {{"result", "query fail"}});
                }
            }
        }
        else if (signal == "find_pw")   // User PW 찾기
        {
            string role = j["role"];
            string name = j["name"];
            string email = j["e_mail"];
            string pnum = j["pnum"];
            if(role=="user") {
                auto escape_quotes = [](const std::string& input) {
                    std::string escaped;
                    for (char c : input) {
                        if (c == '\'') escaped += "\\'";
                        else escaped += c;
                    }
                    return escaped;
                };

                std::string safe_name = escape_quotes(name);
                std::string safe_email = escape_quotes(email);
                std::string safe_pnum = escape_quotes(pnum);

                std::string query = "SELECT PW FROM USER_INFO WHERE NAME = '" + safe_name + "' AND EMAIL = '" + safe_email + "' AND PNUM = '" + safe_pnum + "'";

                if (mysql_query(conn, query.c_str()) == 0)
                {
                    MYSQL_RES* result = mysql_store_result(conn);  // 결과 저장
                    MYSQL_ROW row = mysql_fetch_row(result);
                    std::string pw;

                    if (row && row[0]) {
                        pw = row[0];
                    } else {
                        std::cout << "결과 없음 또는 NULL\n";
                    }
                    if (result != nullptr)
                    {
                        int num_rows = mysql_num_rows(result);  // 결과 행 개수 확인
                        cout << num_rows;
                        if (num_rows > 0) {
                            send_json(client_sock, {{"signal", "find_id_result"},{"result", "success"}, {"user_id",pw}});
                        } else {
                            send_json(client_sock, {{"signal", "find_id_result"},{"result", "fail"}});
                        }
                        mysql_free_result(result);  // 메모리 해제
                    }
                    else {
                        // SELECT는 성공했지만 결과가 없음 또는 오류 발생
                        send_json(client_sock, {{"result", "error"}});
                    }
                }
                else {
                    // 쿼리 실행 실패
                    send_json(client_sock, {{"result", "query fail"}});
                }
            }
        }
        else if (signal == "update_cart")   // 장바구니 추가
        {
            string role = j["role"];
            if (role == "user")
            {
                cout << "add_cart : " << j << endl;
                string email = j["email"];
                string store_name = j["store_name"];
                string menu = j["menu"];
                string option = j["options"];
                string quantity = j["quantity"];
                string price = j["price"];
                string status = j["status"];


            }
            
        }
        else if (signal == "get_my_info")   // User 정보 확인
        {
            string role = j["role"];
            string email = j["e_mail"];
            auto escape_quotes = [](const std::string& input) {
                    std::string escaped;
                    for (char c : input) {
                        if (c == '\'') escaped += "\\'";
                        else escaped += c;
                    }
                    return escaped;
                };

                std::string safe_email = escape_quotes(email);

                std::string query = "SELECT * FROM USER_INFO WHERE EMAIL = " + safe_email;

                if (mysql_query(conn, query.c_str()) == 0)
                {
                    MYSQL_RES* result = mysql_store_result(conn);  // 결과 저장
                    MYSQL_ROW row = mysql_fetch_row(result);
                    json store = json::array();
                    string email; 
                    string name;
                    string pnum;
                    string birth;

                    if (row != nullptr) {
                        email = row[1];
                        name = row[3];
                        pnum = row[4];
                        birth = row[5];
                    } else {
                        std::cout << "결과 없음 또는 NULL\n";
                    }
                    if (result != nullptr)
                    {
                        int num_rows = mysql_num_rows(result);  // 결과 행 개수 확인
                        cout << num_rows;
                        if (num_rows > 0) {
                            send_json(client_sock, {{"signal", "find_id_result"},{"result", "success"}, {"user_id",email}, {"name", name}, {"pnum", pnum}, {"birth", birth}});
                        } else {
                            send_json(client_sock, {{"signal", "find_id_result"},{"result", "fail"}});
                        }
                        mysql_free_result(result);  // 메모리 해제
                    }
                    else {
                        // SELECT는 성공했지만 결과가 없음 또는 오류 발생
                        send_json(client_sock, {{"result", "error"}});
                    }
                }
                else {
                    // 쿼리 실행 실패
                    send_json(client_sock, {{"result", "query fail"}});
                }
            
        }
        else if (signal == "register_favorite") // 즐겨찾기 추가
        {
            string role = j["role"];

            if (role == "user")
            {
                string email = j["e_mail"];
                string store_name = j["store_name"];

                 auto escape_quotes = [](const std::string& input) {
                    std::string escaped;
                    for (char c : input) {
                        if (c == '\'') escaped += "\\'";
                        else escaped += c;
                    }
                    return escaped;
                };

                std::string safe_email = escape_quotes(email);
                string safe_store_name = escape_quotes(store_name);

                std::string query = "INSERT INTO USER_FAVORITES()";


            }
            
        }
        else if (signal == "get_keyword") // 회원 검색어 저장 및 관리
        {
            string path = "./user_keyword";
            string role = j["role"];
            if (role == "user");
            else
            {
                cout<< "잘못된 role입니다." << endl;
            }

            string email = j["email"]; 
            string keyword = j["keyword"];
            json key = {
                {"email", email},
                {"keyword", keyword},
            };

            string query = "SELECT * FROM KEYWORD WHERE USER_ID = '" + email + "'";

            if (mysql_query(conn, query.c_str()) == 0) {
                MYSQL_RES* result = mysql_store_result(conn);
                MYSQL_ROW row = mysql_fetch_row(result);
                // json store = json::array();

                if (row == nullptr)
                {
                    createEmptyJsonFile_new(email, path, true);
                    // appendMenuItemLatestFirst(email, path, key);
                    string filename = path + "/" + email + ".json";
                    string key_query = "INSERT INTO KEYWORD(USER_ID, KEYWORD_PATH) VALUES('"+email+"','"+filename+"')";
                    mysql_query(conn, key_query.c_str());
                }else if (row != nullptr) {
                    // appendMenuItemLatestFirst(email, path, key);
                }

                std::string result_query = "SELECT * FROM KEYWORD WHERE USER_ID= '"+email+"'";

                if (mysql_query(conn, result_query.c_str()) == 0) {
                    MYSQL_RES* result = mysql_store_result(conn);
                    MYSQL_ROW row;
                    json keyword = json::array();

                    while ((row = mysql_fetch_row(result))) {
                        const char* user_keyword = row[2];  // 파일 이름 저장된 컬럼
                        std::string filename = user_keyword ? user_keyword : "";

                        json k;
                        if (!filename.empty()) {
                            std::ifstream input_file(filename);
                            try {
                                input_file >> k;  // 파일 내용을 JSON으로 파싱
                                input_file.close();
                            } catch (const json::parse_error& e) {
                                std::cerr << "JSON parse error: " << e.what() << std::endl;
                                return;
                            }
                        }

                        keyword.push_back({
                            {"email", row[1] ? row[1] : ""},
                            {"keyword", k},
                        });
                    }
                    mysql_free_result(result);
                    send_json(client_sock, { {"signal", "keyword_result"}, {"keyword_info", keyword} });
                } else {
                    send_json(client_sock, { {"signal", "keyword_result"}, {"message", mysql_error(conn)} });
                }
            }else {
                // send_json(client_sock, { {"status", "response_store_login"}, {"store_login_result", "fail"},{"message", mysql_error(conn)} });
                cout << "QUERY_ERROR!" << endl; 
            }
        } 
        else if (signal == "save_cart") // 메뉴 장바구니에 담기
        {
            string role = j["role"];
            if (role == "user");
            else
            {
                cout<< "오류 발생!" << endl;
            }
            string user_id = j["email"];
            string store_name = j["store_name"];
            string status = j["status"];
            string total_price = j["total_price"];
            json menus = j["menus"];  // 배열로 받기

            string query = "INSERT INTO CARTS(USER_EMAIL, STORE_NAME, TOTAL_PRICE, STATUS)VALUES('"+user_id+"','"+store_name+"','"+total_price+"','"+status+"')";

            if (mysql_query(conn, query.c_str())==0)
            {
                string cart_query = "SELECT CART_ID FROM CARTS WHERE USER_EMAIL = '"+user_id+"'";
                mysql_query(conn, cart_query.c_str());
                MYSQL_RES* result = mysql_store_result(conn);  // 결과 저장
                MYSQL_ROW row = mysql_fetch_row(result);
                std::string cart_id;
                if (row && row[0]) {
                    cart_id = row[0];
                } else {
                    std::cout << "결과 없음 또는 NULL\n";
                }
                int size_ = menus.size();

                for (int i = 0; i < size_; i++)
                {
                    string menu = menus[i]["menu_name"];
                    string option = menus[i]["option"];
                    string price = menus[i]["price"];
                    string quantity = menus[i]["quantity"];

                    string add_cart_details = "INSERT INTO CART_DETAILS(CART_ID, MENU, OPTION, PRICE, QUANTITY) VALUES('"+cart_id+"','"+menu+"','"+option+"','"+price+"','"+quantity+"')";
                    if (mysql_query(conn, add_cart_details.c_str())==0)
                    {
                        cout<< "저장 완료" << endl;
                    }
                    else
                    {
                        cout << "ERROR!" << endl;
                    }
                }
                mysql_free_result(result);
            }
            else
            {
                cout << "ERROR!" << endl;
            }
            string carts = "SELECT * FROM CARTS WHERE USER_EMAIL = '"+user_id+"'";
            json cart_info;
            if (mysql_query(conn, carts.c_str())==0)
            {
                MYSQL_RES* result = mysql_store_result(conn);
                MYSQL_ROW row = mysql_fetch_row(result);

                cart_info.push_back({
                    {"store_name", row[2] ? row[2] : ""},
                    {"total_price", row[3] ? row[3] : ""},
                    {"eat_type", row[4] ? row[4] : ""}, 
                });
                mysql_free_result(result);
            }

            string view_cart = "SELECT c.STORE_NAME, c.TOTAL_PRICE,c.STATUS, d.MENU,d.OPTION,d.PRICE,d.QUANTITY FROM CARTS c JOIN CART_DETAILS d ON c.CART_ID = d.CART_ID WHERE c.USER_EMAIL = '"+user_id+"'";
            if (mysql_query(conn, view_cart.c_str()) == 0) {
                MYSQL_RES* result = mysql_store_result(conn);
                MYSQL_ROW row;
                
                json cart_de;
                map<json, json> carts_all;
                while ((row = mysql_fetch_row(result))){

                    cart_de.push_back({
                        {"menu", row[3] ? row[3] : ""},
                        {"option", row[4] ? row[4] : ""},
                        {"price", row[5] ? row[5] : ""},
                        {"quantity", row[6] ? row[6] : ""}
                    });

                }
                mysql_free_result(result);
                cout << "cart_info : " << cart_info << endl;
                send_json(client_sock, {{"signal", "cart_data"}, {"carts", cart_info}, {"cart_details", cart_de} });
            }
            else
            {
                 send_json(client_sock, {{"signal", "cart_data"}, {"message", "error"} });
            }
            
        }    
        else if (signal == "to_pay") // 장바구니 주문 결제
        {
            string role = j["role"];
            string email =  j["email"];
            string pnum = j["user_pnum"];
            string delivery_num = generate_random_code(6);
            json user_addrs= j["user_addr"];
            string user_addr = user_addrs.dump();
            string store_name = j["store_name"];
            string business_num = j["business_num"];
            string total_price = j["total_price"];
            string eat_type = j["eat_type"];
            string to_owner = j["to_owner"];
            string once_plastic = j["once_plastic"];
            string to_rider = j["to_rider"];
            string time = getCurrentTimestamp();
            string accept = "확인 중";
            string time_taken = "0";
            string deliver_status = "대기";
            string rider_status = "대기";
            string order_result = "대기";
            // json menu = j["menu"];
            string menus = j["menus"].dump();

            if (role != "user")
            {
                cout<< "잘못 된 접근 입니다!!!!" << endl;
                return;
            }

            string query = "INSERT INTO ORDER_INFO(DELIVERY_NUM,EMAIL,PNUM,USER_ADDR,STORE_NAME,BUSINESS_NUM,TOTAL_PRICE,EAT_TYPE,ONCE_PLASTIC,"
            "TO_OWNER,TO_RIDER,MENUS,TIME,ACCEPT, TIME_TAKEN,DELIVERY_STATUS, RIDER_STATUS,ORDER_RESULT)" 
            "VALUES('"+delivery_num+"','"+email+"','"+pnum+"','"+user_addr+"','"+store_name+"','"+business_num+"',"
            "'"+total_price+"','"+eat_type+"','"+once_plastic+"','"+to_owner+"','"+to_rider+"',"
            "'"+menus+"','"+time+"','"+accept+"','"+time_taken+"','"+deliver_status+"','"+rider_status+"','"+order_result+"')";

            if (mysql_query(conn, query.c_str())==0)
            {
                cout << "정상 작동" << endl;
            }

            json client_order;

            client_order={
                {"order_serialNumber", delivery_num},
                {"order_time", time},
                {"to_owner", to_owner},
                {"to_rider", to_rider},
                {"once_plastic", once_plastic},
                {"user_pnum", pnum},
                {"user_addr", user_addr},
                {"order_status", deliver_status},
                {"store_name", store_name},
                {"total_price", total_price},
                {"eat_type", eat_type},
                {"order_items", j["menus"]}
            };

            
            int socket_fd = get_socket_for_ip("10.10.20.116");
            if (socket_fd != -1) {
                send_json(socket_fd, {{"signal", "order_accept"}, {"role", "server"}, {"orders", client_order}});
            }
            client_order.clear();
           
            
        }
        else if (signal == "res_order_accept")  // STORE 주문 승낙/거절
        {
            string role = j["role"];
            if (role != "store")
            {
                return;
            }
            cout << "here:" << j << endl;
            string delivery_num = j["order_serialNumber"];
            string store_name = j["store_name"];
            string accept = j["accept"];
            // string delivery_status = "주문 완료";
            string time_taken = j["time_take"];
            if (accept == "yes")
            {
                cout << "here" << endl;
        
                string yes_query = "UPDATE ORDER_INFO SET DELIVERY_STATUS = '주문 수락', ACCEPT = '승인', TIME_TAKEN = '"+time_taken+"',"
                "ORDER_RESULT = '진행' WHERE STORE_NAME = '"+store_name+"' AND DELIVERY_NUM = '"+delivery_num+"'";

                mysql_query(conn, yes_query.c_str());
                int client_so = get_socket_for_ip("10.10.20.118");
                cout << client_so <<endl;
                send_json(client_so, {{"signal", "order_pass"}, {"result", "yes"},{"order_num", delivery_num},{"cooking_time", time_taken}});
            }
            else if (accept == "no")
            {
                string delivery_status = "주문 거절";
                string time_taken = "0";
        
                string no_query = "UPDATE ORDER_INFO SET DELIVERY_STATUS = '주문 거절', ACCEPT = '거절', TIME_TAKEN = '"+time_taken+"',"
                "ORDER_RESULT = '거절' WHERE STORE_NAME = '"+store_name+"' AND DELIVERY_NUM = '"+delivery_num+"'";

                mysql_query(conn, no_query.c_str());
                int client_so = get_socket_for_ip("10.10.20.106");
                send_json(client_so, {{"signal", "order_pass"}, {"result", "no"}, {"order_num", delivery_num}});
            }
        }
        else if (signal == "req_rider_info")    // 조리 완료 시 라이더에게 신호 쀵
        {
            string role = j["role"];
            if (role != "store")
            {
                return;
            }
            cout << "here!" << endl;
            string store_name = j["store_name"];
            string delivery_num = j["order_serialNumber"];
            cout << "here:" << store_name << endl;
            cout << "here:" << delivery_num << endl;
            string query = "UPDATE ORDER_INFO SET ORDER_RESULT = '조리 완료' WHERE STORE_NAME = '"+store_name+"' AND DELIVERY_NUM = '"+delivery_num+"'";

            if (mysql_query(conn, query.c_str())==0)
            {
                cout << "조리 완료" << endl;
                int rider_so = get_socket_for_ip("10.10.20.110");
                
                send_json(rider_so, {{"signal", "yoo_rider"}, {"result","yes_or_no"}, {"store_name", store_name}, {"delivery_num", delivery_num}});
            }

            // string delivery_query = "SELECT * FROM ORDER_INFO WHERE STORE_NAME = '"+store_name+"' AND DELIVERY_NUM = '"+delivery_num+"'";
            // json delivery;
            // if (mysql_query(conn, delivery_query.c_str())==0)
            // {
            //     MYSQL_RES* delivery_result = mysql_store_result(conn);
            //     MYSQL_ROW row = mysql_fetch_row(delivery_result);

            //     delivery = {
            //         {"store_name", row[5] ? row[5] : ""},
            //         {"delivery_num", row[1] ? row[1] : ""}
            //     };
            // }
            // json delivery;
            // delivery = {
            //     {"store_name", store_name},
            //     {"delivery_num", delivery_num}
            // };
 
        }
        else if (signal == "yoo_rider_yes")
        {
            
        }
        
        else if (signal == "register_addr") // 주소 등록
        {
            string role = j["role"];
            if (role == "user") {
                cout << "user connect" << endl;
            }

            string email = j["email"];
            string basic_addr = j["basic"];
            string detail_addr = j["detail"];
            string direction = j["direction"];
            string type = j["type"];
            string is_primary = j["is_primary"];

            string query = "INSERT INTO USER_ADDR(EMAIL, BASIC_ADDR, DETAIL_ADDR, DIRECTION, IS_PRIMARY, TYPE) "
                        "VALUES('" + email + "','" + basic_addr + "','" + detail_addr + "','" + direction + "','" + is_primary + "','" + type + "')";

            if (mysql_query(conn, query.c_str()) == 0) {
                mysql_store_result(conn);  // INSERT 이후에 불필요하지만 유지한다면 ok

                string add_query = "SELECT * FROM USER_ADDR WHERE EMAIL = '" + email + "'";

                if (mysql_query(conn, add_query.c_str()) == 0) {
                    MYSQL_RES* result = mysql_store_result(conn);
                    MYSQL_ROW row;
                    json addr = json::array();

                    // 모든 row 반복하며 JSON 배열에 추가
                    while ((row = mysql_fetch_row(result))) {
                        addr.push_back({
                            {"addr_id", row[0] ? std::to_string(atoi(row[0])) : ""},
                            {"email", row[1] ? row[1] : ""},
                            {"basic_addr", row[2] ? row[2] : ""},
                            {"detail_addr", row[3] ? row[3] : ""},
                            {"direction", row[4] ? row[4] : ""},
                            {"is_primary", row[5] ? row[5] : ""},
                            {"type", row[6] ? row[6] : ""}
                        });
                    }

                    mysql_free_result(result);

                    send_json(client_sock, {
                        {"signal", "get_addr_result"},
                        {"user_addr", addr}
                    });
                } else {
                    send_json(client_sock, {
                        {"signal", "get_addr_result"},
                        {"message", mysql_error(conn)}
                    });
                }
            }
            
        }
        else if (signal == "update_addr")   // 주소지 업데이트
        {
            string role = j["role"];
            if (role != "user"){
                return;
            }
            string email = j["email"];
            string detail_addr = j["detail"];
            string direction = j["direction"];
            string type = j["type"];
            string is_primary = j["is_primary"];


        }
        else if (signal == "del_addr")  // 주소지 삭제
        {
            string role = j["role"];
            if (role != "user")
            {
                return;
            }
            string addr_id = j["addr_id"];
            string email = j["email"];
            cout << "id:" << addr_id <<endl;
            cout << "email:" << email <<endl;

            string query = "DELETE FROM USER_ADDR WHERE ADDR_ID = '"+addr_id+"' AND EMAIL = '"+email+"'";

            if (mysql_query(conn, query.c_str())==0)
            {
                cout << "삭제 완료" << endl;
            }
            else
            {
                cout << "실패!" << endl;
            }

        }
        
        else if (signal == "register_review") // 리뷰 등록
        {
            string role = j["role"];
            if (role == "user")
            {
                
            }
        }
        
        else 
        {
            send_json(client_sock, {{"status", "error"},{"message", "Unknown signal"}});
        }

        mysql_close(conn);

    } catch (const json::parse_error& e) {
        send_json(client_sock, { {"status", "error"}, {"message", string("JSON parse error: ") + e.what()} });
    } catch (const json::type_error& e) {
        send_json(client_sock, { {"status", "error"}, {"message", string("JSON type error: ") + e.what()} });
    }
}

/////////////////////////////////////
// 클라이언트 요청 처리 스레드
/////////////////////////////////////
// void client_worker(int client_sock) {

//     char buffer[2048];
//     while (true) {
//         ssize_t len = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
//         if (len <= 0) break;
//         buffer[len] = '\0';
//         std::string data(buffer);
//         handle_json_message(data, client_sock);
//     }
//     close(client_sock);
//     std::lock_guard<std::mutex> lock(cout_mutex);
//     cout << "[INFO] Client disconnected\n";
// }
// void client_worker(int client_sock) {
//     char buffer[2048];
//     std::string recv_buffer;
//     socket_info s_info;
//     while (true) {
//         ssize_t len = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
//         if (len <= 0) break;

//         buffer[len] = '\0';
//         recv_buffer.append(buffer, len);

//         size_t pos;
//         while ((pos = recv_buffer.find('\n')) != std::string::npos) {
//             std::string json_str = recv_buffer.substr(0, pos);
//             recv_buffer.erase(0, pos + 1);
//             handle_json_message(json_str, client_sock);  // 문자열로 전달
//         }
//     }

//     close(client_sock);
//     std::lock_guard<std::mutex> lock(cout_mutex);
//     std::cout << "[INFO] Client disconnected\n";
// }


// /////////////////////////////////////
// // 서버 초기화 및 수신 루프
// /////////////////////////////////////
// void start_server() {
//     int server_fd = socket(AF_INET, SOCK_STREAM, 0);
//     if (server_fd == -1) {
//         cerr << "Failed to create socket\n";
//         return;
//     }

//     sockaddr_in server_addr{};
//     server_addr.sin_family = AF_INET;
//     server_addr.sin_addr.s_addr = INADDR_ANY;
//     server_addr.sin_port = htons(PORT);

//     if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
//         cerr << "Bind failed\n";
//         close(server_fd);
//         return;
//     }

//     if (listen(server_fd, 10) < 0) {
//         cerr << "Listen failed\n";
//         close(server_fd);
//         return;
//     }

//     cout << "Server listening on port " << PORT << endl;

//     while (true) {
//         sockaddr_in client_addr{};
//         socklen_t client_len = sizeof(client_addr);
//         int client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

//         if (client_sock < 0) {
//             cerr << "Accept failed\n";
//             continue;
//         }
//         else
//         {
//             std::lock_guard<std::mutex> lock(cout_mutex);
//             cout << "[INFO] New client connected\n";
            
//         }

//         std::thread(client_worker, client_sock).detach();
//     }

//     close(server_fd);
// }

void client_worker(int client_sock, std::string client_ip) {
    char buffer[2048];
    std::string recv_buffer;

    {
        std::lock_guard<std::mutex> lock(ip_map_mutex);
        ip_to_socket[client_ip] = client_sock;  // 소켓 저장
        std::lock_guard<std::mutex> lock2(cout_mutex);
        std::cout << "[INFO] Connected IP: " << client_ip << std::endl;
    }

    while (true) {
        ssize_t len = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0) break;

        buffer[len] = '\0';
        recv_buffer.append(buffer, len);

        size_t pos;
        while ((pos = recv_buffer.find('\n')) != std::string::npos) {
            std::string json_str = recv_buffer.substr(0, pos);
            recv_buffer.erase(0, pos + 1);
            handle_json_message(json_str, client_sock);
        }
    }

    close(client_sock);

    {
        std::lock_guard<std::mutex> lock(ip_map_mutex);
        ip_to_socket.erase(client_ip);  // 접속 해제 시 삭제
    }

    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << "[INFO] Client disconnected: " << client_ip << std::endl;
}

void start_server() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr{};

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);  // 원하는 포트

    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, 10);

    std::cout << "Server listening on port "<< PORT << endl;

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

        if (client_sock < 0) continue;

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
        std::string client_ip(ip_str);

        std::thread(client_worker, client_sock, client_ip).detach();
    }

    close(server_fd);
}

void createEmptyJsonFile(const std::string& name) {
    std::string filename = "MENU_"+name+".json";

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "파일을 생성할 수 없습니다: " << filename << std::endl;
        return;
    }

    // 내용 없이 바로 닫음
    file.close();
    std::cout << filename << " 빈 파일이 생성되었습니다." << std::endl;
}

// JSON 파일 생성 함수
void createEmptyJsonFile_new(const std::string& name, const std::string& path, bool asArray) {
    std::filesystem::create_directories(path);  // 경로 없으면 생성

    std::string filename = path + "/" + name + ".json";

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "파일을 생성할 수 없습니다: " << filename << std::endl;
        return;
    }

    json empty = asArray ? json::array() : json::object();  // [] 또는 {}

    file << empty.dump(4);  // 들여쓰기 포함 저장
    file.close();

    std::cout << filename << " 파일이 생성되었습니다." << std::endl;
}

// 현재 시간 반환
string getCurrentTimeString() {
    std::time_t now = std::time(nullptr);
    char buf[100];
    std::strftime(buf, sizeof(buf), "%F %T", std::localtime(&now));
    return std::string(buf);
}

// 메뉴 항목 추가 및 삭제 (최신순 저장 포함)
void appendMenuItemLatestFirst(const std::string& name, const std::string& path, const json& newItem) {
    std::filesystem::create_directories(path);
    std::string filename = path + "/" + name + ".json";

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

string category_name(int category) {

    string menu_name;
    if (category == 101)
    {
        menu_name = "간식";
    } 
    else if (category == 102)
    {
        menu_name = "구이";
    }
    else if (category == 103)
    {
        menu_name = "도시락";
    }
    else if (category == 104)
    {
        menu_name = "돈까스";
    }
    else if (category == 105)
    {
        menu_name = "디저트";
    }
    else if (category == 106)
    {
        menu_name = "멕시칸";
    }
    else if (category == 107)
    {
        menu_name = "버거";
    }
    else if (category == 108)
    {
        menu_name = "분식";
    }
    else if (category == 109)
    {
        menu_name = "샌드위치";
    }
    else if (category == 110)
    {
        menu_name = "샐러드";
    }
    else if (category == 111)
    {
        menu_name = "아시안";
    }
    else if (category == 112)
    {
        menu_name = "양식";
    }
    else if (category == 113)
    {
        menu_name = "일식";
    }
    else if (category == 114)
    {
        menu_name = "족발/보쌈";
    }
    else if (category == 115)
    {
        menu_name = "죽";
    }
    else if (category == 116)
    {
        menu_name = "중식";
    }
    else if (category == 117)
    {
        menu_name = "찜/탕";
    }
    else if (category == 118)
    {
        menu_name = "치킨";
    }
    else if (category == 119)
    {
        menu_name = "커피/차";
    }
    else if (category == 120)
    {
        menu_name = "피자";
    }
    else if (category == 121)
    {
        menu_name = "한식";
    }
    else if (category == 122)
    {
        menu_name = "회/해물";
    }

    return menu_name;

}

// JSON 문자열 또는 일반 텍스트 메시지를 전송
int get_socket_for_ip(const std::string& ip) {
    std::lock_guard<std::mutex> lock(ip_map_mutex);
    auto it = ip_to_socket.find(ip);
    if (it != ip_to_socket.end()) {
        std::cout << "[INFO] Socket for IP " << ip << " : " << it->second << std::endl;
        return it->second;
    } else {
        std::cout << "[WARN] No socket found for IP " << ip << std::endl;
        return -1; // 실패 시 -1 반환
    }
}

// 랜덤한 난수 생성
std::string generate_random_code(int length) 
{
    if (length < 3) throw std::invalid_argument("Length must be at least 3 to include 3 digits");

    const std::string digits = "0123456789";
    const std::string letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> digit_dis(0, digits.size() - 1);
    std::uniform_int_distribution<> letter_dis(0, letters.size() - 1);

    std::string code;

    // 1. 숫자 3개 삽입
    for (int i = 0; i < 3; ++i) {
        code += digits[digit_dis(gen)];
    }

    // 2. 나머지 자릿수는 영문자로 채움
    for (int i = 0; i < length - 3; ++i) {
        code += letters[letter_dis(gen)];
    }

    // 3. 전체를 랜덤하게 섞기
    std::shuffle(code.begin(), code.end(), gen);

    return code;
}

// 현재 시간 표현 함수
string getCurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm* local_time = std::localtime(&now);

    std::ostringstream oss;
    oss << std::put_time(local_time, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// 년-월-일 표시
string getCurrentDateOnly() {
    std::time_t now = std::time(nullptr);
    std::tm* local_time = std::localtime(&now);

    std::ostringstream oss;
    oss << std::put_time(local_time, "%Y-%m-%d");  // 시간 제외
    return oss.str();
}


//////////////////////
// 프로그램 시작점
//////////////////////
int main() {
    start_server();
    return 0;
}
