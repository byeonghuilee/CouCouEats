// 표준 및 외부 라이브러리 헤더
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <string>
#include <netinet/in.h>    // 소켓 프로그래밍
#include <unistd.h>        // close(), read(), write()
#include "./json.hpp" // JSON 파싱
#include <mariadb/mysql.h> // MariaDB 연결
#include <unistd.h>
#include <fstream>


using json = nlohmann::json;
using namespace std;
void createEmptyJsonFile(const std::string& name);
// 출력 시 동시 접근 방지를 위한 전역 뮤텍스
std::mutex cout_mutex;
std::string buffer;  // 누적 버퍼
// 서버가 리슨할 포트 정의
constexpr int PORT = 20008;


struct socket_info
{
    string id;
    string socket;
};


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
    if (!mysql_real_connect(conn, "localhost", "root", "1111", "coucoueats", 0, NULL, 0)) {//if (!mysql_real_connect(conn, "10.10.20.118", "LBH", "1234", "CouCouEats", 0, NULL, 0)) {
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
            send_json(client_sock, { {"status", "error"}, {"message", "'signal' field missing"} });
            return;
        }
        
        // socket_info s_info;
        // std::string who = j["who"];
        // s_info.id.append(who);
        // s_info.socket.append(to_string(client_sock));

        std::string signal = j["signal"];
        cout << "signal:" << j<< endl;
        MYSQL* conn = connect_db();
        if (!conn) return;

        if (signal == "create") {
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
        else if (signal == "search") {
            std::string query = "SELECT * FROM STORE";
            if (mysql_query(conn, query.c_str()) == 0) {
                MYSQL_RES* result = mysql_store_result(conn);
                MYSQL_ROW row;
                json store = json::array();

                while ((row = mysql_fetch_row(result))) {
                    const char* origin_info_ptr = row[13];  // 파일 이름 저장된 컬럼
                    std::string filename = origin_info_ptr ? origin_info_ptr : "";

                    json j;
                    if (!filename.empty()) {
                        std::ifstream input_file(filename);
                        try {
                            input_file >> j;  // 파일 내용을 JSON으로 파싱
                            input_file.close();
                        } catch (const json::parse_error& e) {
                            std::cerr << "JSON parse error: " << e.what() << std::endl;
                            return;
                        }
                    }

                    int category = 0;
                    if (row[1]) {
                        try {
                            category = std::stoi(row[1]);
                        } catch (const std::exception& e) {
                            category = 0; // fallback
                        }
                    }

                    store.push_back({
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
                    });
                }
                mysql_free_result(result);
                send_json(client_sock, { {"signal", "r"}, {"users", store} });
            } else {
                send_json(client_sock, { {"status", "error"}, {"message", mysql_error(conn)} });
            }
        }
            // if (mysql_query(conn, query.c_str()) == 0) {
            //     MYSQL_RES* result = mysql_store_result(conn);
            //     MYSQL_ROW row;
            //     json store = json::array();
            //     while ((row = mysql_fetch_row(result))) {
            //         cout<<row[13];
            //         const std::string filename = row[13];
            //         json j;
            //         std::ifstream input_file(filename);

            //         try {
            //             input_file >> j;  // 파일 내용을 JSON으로 파싱
            //             input_file.close();
            //         } catch (const json::parse_error& e) {
            //             std::cerr << "JSON parse error: " << e.what() << std::endl;
            //             return;
            //         }
                    
            //         // store.push_back({
            //         //     {"store_name", row[2]},
            //         //     {"store_addr", row[3]},
            //         //     {"phone_num", row[4]},
            //         //     {"owner_name", row[5]},
            //         //     {"company_number", row[6]},
            //         //     {"opening_time",row[7]},
            //         //     {"closing_time", row[8]},
            //         //     {"delivery_fee", row[9]},
            //         //     {"minimum_order", row[10]},
            //         //     {"store_desc", row[11]},
            //         //     // {"menu_info", j},
            //         //     {"origin_info", row[13]},
            //         //     {"nutrition_fact", row[14]},
            //         //     {"allergen_info", row[15]},
            //         //     {"stataus", row[16]}
            //         // });
            //     }
            //     mysql_free_result(result);
            //     send_json(client_sock, { {"signal", "r"}, {"users", store} });
            // } else {
            //     send_json(client_sock, { {"status", "error"}, {"message", mysql_error(conn)} });
            // }

        // } 
        else if (signal == "update") {
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
        else if (signal == "delete") {
            int id = j["id"];
            std::string query = "DELETE FROM users WHERE id=" + to_string(id);
            if (mysql_query(conn, query.c_str()) == 0) {
                send_json(client_sock, { {"status", "success"}, {"message", "User deleted"} });
            } else {
                send_json(client_sock, { {"status", "error"}, {"message", mysql_error(conn)} });
            }

        }
        else if (signal == "sign_up_result")
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
        else if (signal == "sign_up")
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
                
                if (mysql_query(conn, query.c_str()) == 0)
                {
                    
                    
                    if (mysql_query(conn, query.c_str()) == 0) {
                        // send_json(client_sock, { {"status", "sign_up"}, {"result", "회원가입 완료!"}, {"user_id", user_id}, {"e-mail", u_email}});
                        cout << "회원가입 완료" << endl;
                    } else {
                        // send_json(client_sock, { {"status", "error"}, {"result", "회원가입에 실패 하셨습니다."} });
                        cout << "회원가입 실패" << endl;
                    }
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
        else if (signal == "login")
        {
            string role = j["role"];
            
            json user_cart = json::array();

            if (role == "user") {
                string email = j["e_mail"];
                string pw = j["pw"];
                // 1. 로그인 확인
                string login_query = "SELECT * FROM USER_INFO WHERE EMAIL = '" + email + "' AND PW = '" + pw + "'";

                if (mysql_query(conn, login_query.c_str()) == 0)
                {
                    MYSQL_RES* result = mysql_store_result(conn);
                    MYSQL_ROW row = mysql_fetch_row(result);

                    if (row != nullptr)
                    {
                        // 로그인 성공 → 사용자 정보 추출
                        string user_id = row[0] ? row[0] : "";
                        string u_email = row[1] ? row[1] : "";
                        string name = row[3] ? row[3] : "";
                        string pnum = row[4] ? row[4] : "";

                        mysql_free_result(result);  // 사용자 쿼리 결과 해제

                        // 2. 장바구니 정보 조회
                        string cart_query =
                            "SELECT c.CART_ID, c.USER_EMAIL, c.STORE_NAME, c.TOTAL_PRICE, c.STATUS, "
                            "d.CD_ID, d.MENU, d.OPTION, d.PRICE, d.QUANTITY "
                            "FROM CARTS c JOIN CART_DETAILS d ON c.CART_ID = d.CART_ID "
                            "WHERE c.USER_EMAIL = '" + email + "'";

                        if (mysql_query(conn, cart_query.c_str()) == 0)
                        {
                            MYSQL_RES* cart_result = mysql_store_result(conn);
                            MYSQL_ROW cart_row;

                            while ((cart_row = mysql_fetch_row(cart_result)) != nullptr)
                            {
                                int cart_id = 0;
                                if (cart_row[0]) {
                                    try {
                                        cart_id = std::stoi(cart_row[0]);
                                    } catch (const std::exception& e) {
                                        std::cerr << "CART_ID 변환 오류: " << e.what() << std::endl;
                                        cart_id = 0;
                                    }
                                }

                                json cart_item = {
                                    {"CART_ID", cart_id},
                                    {"USER_EMAIL", cart_row[1] ? cart_row[1] : ""},
                                    {"STORE_NAME", cart_row[2] ? cart_row[2] : ""},
                                    {"TOTAL_PRICE", cart_row[3] ? cart_row[3] : ""},
                                    {"STATUS", cart_row[4] ? cart_row[4] : ""},
                                    {"CD_ID", cart_row[5] ? cart_row[5] : ""},
                                    {"MENU", cart_row[6] ? cart_row[6] : ""},
                                    {"OPTION", cart_row[7] ? cart_row[7] : ""},
                                    {"PRICE", cart_row[8] ? cart_row[8] : ""},
                                    {"QUANTITY", cart_row[9] ? cart_row[9] : ""}
                                };

                                user_cart.push_back(cart_item);
                            }

                            mysql_free_result(cart_result);
                        } else {
                            std::cerr << "장바구니 쿼리 실패: " << mysql_error(conn) << std::endl;
                        }

                        // 3. 응답 생성 (조건부 cart 포함)
                        json response = {
                            {"signal", "login"},
                            {"result", "success"},
                            {"user_id", user_id},
                            {"u_email", u_email},
                            {"name", name},
                            {"pnum", pnum}
                        };

                        if (!user_cart.empty()) {
                            response["cart"] = user_cart;
                        }

                        send_json(client_sock, response);
                    }
                    else
                    {
                        send_json(client_sock, {{"signal", "login"}, {"result", "fail"}});
                        mysql_free_result(result);
                    }
                }
                else
                {
                    send_json(client_sock, {{"signal", "login"}, {"result", "query fail"}});
                }
            }
            else if (role == "store")
            {
                cout << "store:" << j <<endl;
                string store_id = j["id"];
                string pw = j["pw"];
                string login_query = "SELECT PNUM FROM OWNER_INFO WHERE BUSINESS_ID = '" + store_id + "' AND PW = '" + pw + "'";
                
                if (mysql_query(conn, login_query.c_str()) == 0)
                {
                    MYSQL_RES* result = mysql_store_result(conn);  // 결과 저장
                    MYSQL_ROW row = mysql_fetch_row(result);
                    std::string pnum;

                    if (row && row[0]) {
                        pnum = row[0];
                    } else {
                        std::cout << "결과 없음 또는 NULL\n";
                    }
                    cout << "pnum:"<<pnum<<endl;
                    std::string query2 = "SELECT * FROM STORE";

                    if (mysql_query(conn, query2.c_str()) == 0) {
                        MYSQL_RES* result = mysql_store_result(conn);
                        MYSQL_ROW row;
                        json store = json::array();

                        while ((row = mysql_fetch_row(result))) {
                            const char* origin_info_ptr = row[13];  // 파일 이름 저장된 컬럼
                            std::string filename = origin_info_ptr ? origin_info_ptr : "";

                            json j;
                            if (!filename.empty()) {
                                std::ifstream input_file(filename);
                                try {
                                    input_file >> j;  // 파일 내용을 JSON으로 파싱
                                    input_file.close();
                                } catch (const json::parse_error& e) {
                                    std::cerr << "JSON parse error: " << e.what() << std::endl;
                                    return;
                                }
                            }

                            int category = 0;
                            if (row[1]) {
                                try {
                                    category = std::stoi(row[1]);
                                } catch (const std::exception& e) {
                                    category = 0; // fallback
                                }
                            }

                            store.push_back({
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
                            });
                        }
                        mysql_free_result(result);
                        send_json(client_sock, { {"signal", "store_login"}, {"store_info", store} });
                    } else {
                        send_json(client_sock, { {"status", "error"}, {"message", mysql_error(conn)} });
                    }

                }
            }
            
        }

        // else if (signal == "login")
        // {
        //     string role = j["role"];
        //     string email = j["e_mail"];
        //     string pw = j["pw"];
        //     json user_cart = json::array();;
        //     if (role == "user") {
                
        //         string query = "SELECT * FROM USER_INFO WHERE EMAIL = '" + email + "' AND PW = '" + pw + "'";
        //         string query2 = "SELECT c.CART_ID, c.USER_EMAIL, c.STORE_NAME, c.TOTAL_PRICE,c.STATUS,d.CD_ID,d.MENU,d.OPTION,d.PRICE,d.QUANTITY FROM CARTS cJOIN CART_DETAILS d ON c.CART_ID = d.CART_ID WHERE c.USER_EMAIL = '"+email+"'";

                

        //         if (mysql_query(conn, query2.c_str()) == 0)
        //         {
        //             MYSQL_RES* result = mysql_store_result(conn);
        //             MYSQL_ROW row = mysql_fetch_row(result);
        //             while ((row = mysql_fetch_row(result)) != nullptr)
        //             {
        //                 int cart_id = 0;
        //                 if (row[0]) {
        //                     try {
        //                         cart_id = std::stoi(row[0]);
        //                     } catch (...) {
        //                         cart_id = 0;
        //                     }
        //                 }

        //                 json cart_item = {
        //                     {"CART_ID", cart_id},
        //                     {"USER_EMAIL", row[1] ? row[1] : ""},
        //                     {"STORE_NAME", row[2] ? row[2] : ""},
        //                     {"TOTAL_PRICE", row[3] ? row[3] : ""},
        //                     {"STATUS", row[4] ? row[4] : ""},
        //                     {"CD_ID", row[5] ? row[5] : ""},
        //                     {"MENU", row[6] ? row[6] : ""},
        //                     {"OPTION", row[7] ? row[7] : ""},
        //                     {"PRICE", row[8] ? row[8] : ""},
        //                     {"QUANTITY", row[9] ? row[9] : ""}
        //                 };

        //                 user_cart.push_back(cart_item);
        //             }

        //             mysql_free_result(result);
        //         }
        //         else
        //         {
        //             std::cerr << "쿼리 실패: " << mysql_error(conn) << std::endl;
        //         }
                

        //         if (mysql_query(conn, query.c_str()) == 0)
        //         {
        //             MYSQL_RES* result = mysql_store_result(conn);  // 결과 저장
        //             MYSQL_ROW row = mysql_fetch_row(result);
        //             std::string user_id ;
        //             string u_email;
        //             string pnum;
        //             string name;
        //             if (row!=nullptr) {
        //                 user_id = row[0] ? row[0] : "";
        //                 u_email = row[1] ? row[1] : "";
        //                 name = row[3] ? row[3] : "";
        //                 pnum = row[4] ? row[4] : "";
        //             } else {
        //                 std::cout << "결과 없음 또는 NULL\n";
        //             }
        //             if (result != nullptr)
        //             {
        //                 int num_rows = mysql_num_rows(result);  // 결과 행 개수 확인
        //                 cout << num_rows;
        //                 if (num_rows > 0) {
        //                     send_json(client_sock, {{"signal", "login"},{"result", "success"}, {"user_id", user_id}, {"u_email", u_email}, {"name", name}, {"pnum", pnum}, {"cart", user_cart}});
        //                 } else {
        //                     send_json(client_sock, {{"signal", "login"},{"result", "fail"}});
        //                 }
        //                 mysql_free_result(result);  // 메모리 해제
        //             }
        //             else {
        //                 // SELECT는 성공했지만 결과가 없음 또는 오류 발생
        //                 send_json(client_sock, {{"result", "error"}});
        //             }
        //         }
        //         else {
        //             // 쿼리 실행 실패
        //             send_json(client_sock, {{"result", "query fail"}});
        //         }
        //     }
            
        // }
        else if (signal == "find_id")
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
        else if (signal == "find_pw")
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
        else if (signal == "update_cart")
        {
            string role = j["role"];
            if (role == "user")
            {
                string email = j["email"];
                string store_name = j["store_name"];
                string menu = j["menu"];
                string option = j["options"];
                string quantity = j["quantity"];
                string price = j["price"];
                string status = j["status"];
            }
            
        }
        else if (signal == "get_my_info")
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
        else if (signal == "register_favorite")
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
        
        else {
            send_json(client_sock, { {"status", "error"}, {"message", "Unknown signal"} });
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
void client_worker(int client_sock) {
    char buffer[2048];
    std::string recv_buffer;
    socket_info s_info;
    while (true) {
        ssize_t len = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (len <= 0) break;

        buffer[len] = '\0';
        recv_buffer.append(buffer, len);

        size_t pos;
        while ((pos = recv_buffer.find('\n')) != std::string::npos) {
            std::string json_str = recv_buffer.substr(0, pos);
            recv_buffer.erase(0, pos + 1);
            handle_json_message(json_str, client_sock);  // 문자열로 전달
        }
    }

    close(client_sock);
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << "[INFO] Client disconnected\n";
}


/////////////////////////////////////
// 서버 초기화 및 수신 루프
/////////////////////////////////////
void start_server() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        cerr << "Failed to create socket\n";
        return;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Bind failed\n";
        close(server_fd);
        return;
    }

    if (listen(server_fd, 10) < 0) {
        cerr << "Listen failed\n";
        close(server_fd);
        return;
    }

    cout << "Server listening on port " << PORT << endl;

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

        if (client_sock < 0) {
            cerr << "Accept failed\n";
            continue;
        }
        else
        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            cout << "[INFO] New client connected\n";
            
        }

        std::thread(client_worker, client_sock).detach();
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

//////////////////////
// 프로그램 시작점
//////////////////////
int main() {
    start_server();
    return 0;
}
