#include "crow.h"
#include <sqlite3.h>
#include <iostream>
#include <mutex>
#include <filesystem>
#include <fstream>

sqlite3* db;
std::mutex db_mutex;

void init_db(){
    sqlite3_open("zplus.db",&db);
    sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, username TEXT UNIQUE, email TEXT UNIQUE, phone TEXT UNIQUE, password TEXT);",0,0,0);
    sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS reels (id INTEGER PRIMARY KEY, creator TEXT, videoUrl TEXT, description TEXT, likes INTEGER DEFAULT 0);",0,0,0);
    sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS likes (id INTEGER PRIMARY KEY, reel_id INTEGER, username TEXT, UNIQUE(reel_id,username));",0,0,0);
    sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS comments (id INTEGER PRIMARY KEY, reel_id INTEGER, username TEXT, text TEXT);",0,0,0);
    std::filesystem::create_directories("uploads");
    sqlite3_stmt* s; sqlite3_prepare_v2(db,"SELECT COUNT(*) FROM reels;",-1,&s,0); sqlite3_step(s);
    if(sqlite3_column_int(s,0)==0){
        sqlite3_exec(db,"INSERT INTO reels (creator,videoUrl,description,likes) VALUES ('@zplus','https://www.w3schools.com/html/mov_bbb.mp4','Welcome to z+ 🚀 Katsina to the world',1200);",0,0,0);
        sqlite3_exec(db,"INSERT INTO reels (creator,videoUrl,description,likes) VALUES ('@sadeeq','https://www.w3schools.com/html/movie.mp4','First C++ Reel 🔥',500);",0,0,0);
    }
    sqlite3_finalize(s);
    std::cout << "[z+ V6] DB: zplus.db | Uploads: ./uploads/\n";
}
std::string hash_pass(std::string p){ return std::to_string(std::hash<std::string>{}(p+"zplus_salt_2026")); }

struct Cors{ struct context{}; void before_handle(crow::request& req, crow::response& res, context& ctx){ if(req.method==crow::HTTPMethod::Options){res.code=204; res.end();}} void after_handle(crow::request& req, crow::response& res, context& ctx){ res.add_header("Access-Control-Allow-Origin","*"); res.add_header("Access-Control-Allow-Methods","GET, POST, OPTIONS"); res.add_header("Access-Control-Allow-Headers","Content-Type, Authorization"); } };

int main(){
    init_db();
    crow::App<Cors> app;

    CROW_ROUTE(app, "/")([](){ return "z+ V6 COMPLETE - SQLite + Upload + Likes + Comments"; });

    CROW_ROUTE(app, "/api/auth/signup").methods(crow::HTTPMethod::Post)([](const crow::request& req){
        auto j=crow::json::load(req.body); if(!j) return crow::response(400,"{\"error\":\"Invalid\"}");
        std::string u=j["username"].s(), e=j.has("email")?j["email"].s():"", ph=j.has("phone")?j["phone"].s():"", pw=j["password"].s();
        std::lock_guard<std::mutex> lock(db_mutex);
        sqlite3_stmt* st; sqlite3_prepare_v2(db,"INSERT INTO users (username,email,phone,password) VALUES (?,?,?,?);",-1,&st,0);
        sqlite3_bind_text(st,1,u.c_str(),-1,SQLITE_STATIC); sqlite3_bind_text(st,2,e.empty()?nullptr:e.c_str(),-1,SQLITE_STATIC);
        sqlite3_bind_text(st,3,ph.empty()?nullptr:ph.c_str(),-1,SQLITE_STATIC); std::string hp=hash_pass(pw); sqlite3_bind_text(st,4,hp.c_str(),-1,SQLITE_STATIC);
        if(sqlite3_step(st)!=SQLITE_DONE){ sqlite3_finalize(st); return crow::response(409,"{\"error\":\"User exists\"}"); }
        sqlite3_finalize(st); crow::json::wvalue r; r["token"]=u+"_token"; r["username"]=u; return crow::response(201,r);
    });

    CROW_ROUTE(app, "/api/auth/login").methods(crow::HTTPMethod::Post)([](const crow::request& req){
        auto j=crow::json::load(req.body); std::string id=j["username"].s(), pw=j["password"].s();
        std::lock_guard<std::mutex> lock(db_mutex);
        sqlite3_stmt* st; sqlite3_prepare_v2(db,"SELECT username FROM users WHERE (username=? OR email=? OR phone=?) AND password=?;",-1,&st,0);
        sqlite3_bind_text(st,1,id.c_str(),-1,SQLITE_STATIC); sqlite3_bind_text(st,2,id.c_str(),-1,SQLITE_STATIC); sqlite3_bind_text(st,3,id.c_str(),-1,SQLITE_STATIC);
        std::string hp=hash_pass(pw); sqlite3_bind_text(st,4,hp.c_str(),-1,SQLITE_STATIC);
        if(sqlite3_step(st)==SQLITE_ROW){ std::string un=(char*)sqlite3_column_text(st,0); sqlite3_finalize(st); crow::json::wvalue r; r["token"]=un+"_token"; r["username"]=un; return crow::response(200,r); }
        sqlite3_finalize(st); return crow::response(401,"{\"error\":\"Invalid credentials\"}");
    });

    CROW_ROUTE(app, "/api/reels/feed").methods(crow::HTTPMethod::Get)([](){
        std::lock_guard<std::mutex> lock(db_mutex);
        sqlite3_stmt* st; sqlite3_prepare_v2(db,"SELECT id,creator,videoUrl,description,likes FROM reels ORDER BY id DESC;",-1,&st,0);
        std::vector<crow::json::wvalue> list;
        while(sqlite3_step(st)==SQLITE_ROW){ crow::json::wvalue n; n["id"]=sqlite3_column_int(st,0); n["creator"]=std::string((char*)sqlite3_column_text(st,1)); n["videoUrl"]=std::string((char*)sqlite3_column_text(st,2)); n["description"]=std::string((char*)sqlite3_column_text(st,3)); n["likes"]=sqlite3_column_int(st,4); list.push_back(std::move(n)); }
        sqlite3_finalize(st); crow::json::wvalue res; res["feed"]=std::move(list); return crow::response(200,res);
    });

    CROW_ROUTE(app, "/api/upload").methods(crow::HTTPMethod::Post)([](const crow::request& req){
        crow::multipart::message msg(req);
        std::string creator="anon", description="New reel", savedPath="";
        for(auto& part: msg.parts){
            std::string name = part.get_header_object("Content-Disposition").get("name");
            if(name=="creator") creator=part.body;
            else if(name=="description") description=part.body;
            else if(name=="video"){
                std::string filename = part.get_header_object("Content-Disposition").get("filename");
                if(filename.empty()) filename="video.mp4";
                filename = std::filesystem::path(filename).filename().string();
                savedPath = "uploads/" + std::to_string(std::time(nullptr)) + "_" + filename;
                std::ofstream out(savedPath, std::ios::binary); out << part.body; out.close();
                std::cout << "[UPLOAD] " << savedPath << " by " << creator << "\n";
            }
        }
        if(savedPath.empty()) return crow::response(400,"{\"error\":\"No file\"}");
        std::lock_guard<std::mutex> lock(db_mutex);
        std::string videoUrl = "http://localhost:8080/" + savedPath;
        sqlite3_stmt* st; sqlite3_prepare_v2(db,"INSERT INTO reels (creator,videoUrl,description,likes) VALUES (?,?,?,0);",-1,&st,0);
        sqlite3_bind_text(st,1,creator.c_str(),-1,SQLITE_STATIC); sqlite3_bind_text(st,2,videoUrl.c_str(),-1,SQLITE_STATIC); sqlite3_bind_text(st,3,description.c_str(),-1,SQLITE_STATIC);
        sqlite3_step(st); sqlite3_finalize(st);
        crow::json::wvalue r; r["videoUrl"]=videoUrl; r["message"]="Uploaded"; return crow::response(201,r);
    });

    CROW_ROUTE(app, "/api/reels/<int>/like").methods(crow::HTTPMethod::Post)([](const crow::request& req, int reel_id){
        auto j=crow::json::load(req.body); std::string username=j["username"].s(); std::lock_guard<std::mutex> lock(db_mutex);
        sqlite3_stmt* st; sqlite3_prepare_v2(db,"SELECT id FROM likes WHERE reel_id=? AND username=?;",-1,&st,0); sqlite3_bind_int(st,1,reel_id); sqlite3_bind_text(st,2,username.c_str(),-1,SQLITE_STATIC);
        bool already=sqlite3_step(st)==SQLITE_ROW; sqlite3_finalize(st);
        if(already){ sqlite3_prepare_v2(db,"DELETE FROM likes WHERE reel_id=? AND username=?;",-1,&st,0); sqlite3_bind_int(st,1,reel_id); sqlite3_bind_text(st,2,username.c_str(),-1,SQLITE_STATIC); sqlite3_step(st); sqlite3_finalize(st); sqlite3_exec(db,("UPDATE reels SET likes=likes-1 WHERE id="+std::to_string(reel_id)+";").c_str(),0,0,0); return crow::response(200,"{\"liked\":false}"); }
        else { sqlite3_prepare_v2(db,"INSERT INTO likes (reel_id,username) VALUES (?,?);",-1,&st,0); sqlite3_bind_int(st,1,reel_id); sqlite3_bind_text(st,2,username.c_str(),-1,SQLITE_STATIC); sqlite3_step(st); sqlite3_finalize(st); sqlite3_exec(db,("UPDATE reels SET likes=likes+1 WHERE id="+std::to_string(reel_id)+";").c_str(),0,0,0); return crow::response(200,"{\"liked\":true}"); }
    });

    CROW_ROUTE(app, "/uploads/<string>")([](crow::request& req, crow::response& res, std::string filename){
        std::string path = "uploads/" + filename; res.set_static_file_info(path); res.end();
    });

    CROW_ROUTE(app, "/api/reels/<int>/comment").methods(crow::HTTPMethod::Post)([](const crow::request& req, int reel_id){
        auto j=crow::json::load(req.body); std::lock_guard<std::mutex> lock(db_mutex); sqlite3_stmt* st; sqlite3_prepare_v2(db,"INSERT INTO comments (reel_id,username,text) VALUES (?,?,?);",-1,&st,0);
        sqlite3_bind_int(st,1,reel_id); sqlite3_bind_text(st,2,j["username"].s().c_str(),-1,SQLITE_STATIC); sqlite3_bind_text(st,3,j["text"].s().c_str(),-1,SQLITE_STATIC); sqlite3_step(st); sqlite3_finalize(st); return crow::response(201,"{\"ok\":true}");
    });

    std::cout << "\n=== z+ V6 FINAL COMPLETE ===\nRun: ./server -> http://localhost:8080\n";
    app.port(8080).multithreaded().run();
}