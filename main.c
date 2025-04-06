#include <stdio.h>
#include <time.h>
#include "sqlite3.h"

int add_user(sqlite3 *db, const char *username, const char *password_hash) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO users (username, password_hash) VALUES (?, ?);";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) return rc;

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password_hash, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : 1;
}

int check_user_exists(sqlite3 *db, const char *username) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id FROM users WHERE username = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    int exists = (rc == SQLITE_ROW) ? 1 : 0;
    sqlite3_finalize(stmt);
    return exists;
}

int register_user(sqlite3 *db, const char *username, const char *password_hash) {
    if (check_user_exists(db, username)) {
        printf("Пользователь уже существует!\n");
        return 1;
    }
    return add_user(db, username, password_hash);
}

int add_candidate(sqlite3 *db, const char *name) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO candidates (name) VALUES (?);";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        printf("Ошибка подготовки запроса для кандидата: %s\n", sqlite3_errmsg(db));
        return rc;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        printf("Error of adding candidate %s: %s\n", name, sqlite3_errmsg(db));
    } else {
        printf("Candidate %s succesfully added!\n", name);
    }
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : 1;
}

int has_voted(sqlite3 *db, int user_id) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id FROM votes WHERE user_id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, user_id);
    rc = sqlite3_step(stmt);
    int voted = (rc == SQLITE_ROW) ? 1 : 0;
    sqlite3_finalize(stmt);
    return voted;
}

int candidate_exists(sqlite3 *db, int candidate_id) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id FROM candidates WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, candidate_id);
    rc = sqlite3_step(stmt);
    int exists = (rc == SQLITE_ROW) ? 1 : 0;
    sqlite3_finalize(stmt);
    return exists;
}

int vote(sqlite3 *db, int user_id, int candidate_id) {
    if (has_voted(db, user_id)) {
        printf("Ошибка: Пользователь уже проголосовал!\n");
        return 1;
    }

    if (!candidate_exists(db, candidate_id)) {
        printf("Ошибка: Кандидат с ID %d не существует!\n", candidate_id);
        return 1;
    }

    sqlite3_stmt *stmt;
    char timestamp[20];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    const char *sql = "INSERT INTO votes (user_id, candidate_id, timestamp) VALUES (?, ?, ?);";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        printf("Ошибка подготовки запроса: %s\n", sqlite3_errmsg(db));
        return rc;
    }

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, candidate_id);
    sqlite3_bind_text(stmt, 3, timestamp, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        printf("Ошибка выполнения запроса: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? 0 : 1;
}

int main() {
    sqlite3 *db;
    char *err_msg = 0;

    int rc = sqlite3_open("voting.db", &db);
    if (rc) {
        fprintf(stderr, "Ошибка открытия БД: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    // Создаём таблицы
    const char *sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT NOT NULL UNIQUE,"
        "password_hash TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS candidates ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS votes ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "user_id INTEGER,"
        "candidate_id INTEGER,"
        "timestamp TEXT,"
        "FOREIGN KEY(user_id) REFERENCES users(id),"
        "FOREIGN KEY(candidate_id) REFERENCES candidates(id));";

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Ошибка создания таблиц: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }

    // Очищаем таблицы перед тестом
    sqlite3_exec(db, "DELETE FROM votes;", 0, 0, &err_msg);
    sqlite3_exec(db, "DELETE FROM users;", 0, 0, &err_msg);
    sqlite3_exec(db, "DELETE FROM candidates;", 0, 0, &err_msg);
    // Сбрасываем счётчик AUTOINCREMENT для candidates
    sqlite3_exec(db, "DELETE FROM sqlite_sequence WHERE name='candidates';", 0, 0, &err_msg);

    // Добавляем тестовых пользователей
    register_user(db, "user1", "hash123");
    register_user(db, "user2", "hash456");

    // Добавляем тестовых кандидатов
    add_candidate(db, "Candidate A");
    add_candidate(db, "Candidate B");
    add_candidate(db, "Candidate C");

    // Выводим всех кандидатов для проверки
    sqlite3_stmt *stmt;
    const char *select_sql = "SELECT id, name FROM candidates;";
    rc = sqlite3_prepare_v2(db, select_sql, -1, &stmt, 0);
    if (rc == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const unsigned char *name = sqlite3_column_text(stmt, 1);
            printf("Candidate: ID=%d, Name=%s\n", id, name);
        }
    }
    sqlite3_finalize(stmt);

    // Голос за существующего кандидата (ID 1) от user1
    rc = vote(db, 1, 1);
    if (rc == 0) {
        printf("Vote is registered!\n");
    } else {
        printf("Error of registering the vote!\n");
    }

    // Голос за несуществующего кандидата (ID 999) от user2
    rc = vote(db, 2, 999);
    if (rc == 0) {
        printf("Vote is registered!\n");
    } else {
        printf("Error of registering the vote!\n");
    }

    sqlite3_close(db);
    return 0;
}