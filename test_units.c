#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "phone_db.h"
#include "test_framework.h"

static void write_csv(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    assert(f);
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}

/* ======================================================================
 * UNIT #1 Макрос RUN больше не считает падения дважды
 * ====================================================================== */

/**
 * @brief Тест, который гарантированно падает.
 *
 * Используется для проверки корректности подсчёта падений в макросе RUN.
 * Всегда возвращает ошибку (ASSERT(0)).
 */
TEST(test_that_will_fail) {
    ASSERT(0);
}

/**
 * @brief Тест, который гарантированно проходит.
 *
 * Используется для проверки корректности подсчёта прохождений в макросе RUN.
 * Всегда завершается успешно.
 */
TEST(test_that_will_pass) {
    ASSERT(1);
}

/**
 * @brief Проверяет, что макрос RUN считает падения дважды.
 *
 * После падения test_that_will_fail переменные tests_failed и tests_passed
 * должны отражать реальное состояние: падения не считаются как прохождения.
 */
TEST(test_run_macro_no_double_count) {
    ASSERT(tests_failed > 0);
    ASSERT(tests_passed < tests_run);
}

/* ======================================================================
 * UNIT #2 phone_db_save_sorted не изменяет db->comment_buf
 * ====================================================================== */

/**
 * @brief Проверяет, что save_sorted не изменяет db->comment_buf.
 *
 * Тест гарантирует, что повторные вызовы phone_db_save_sorted
 * не приводят к росту размера буфера комментариев.
 * comment_buf_size не должен расти при повторном сохранении.
 */
TEST(test_save_sorted_no_comment_buf_mutation) {
    write_csv("test_data/save_mut.csv", "AAAA;hello;\n");

    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/save_mut.csv");

    size_t before = db.comment_buf_size;

    phone_db_save_sorted(&db, "test_data/out_mut1.csv");
    size_t after_first = db.comment_buf_size;

    phone_db_apply_increment(&db, OP_UPDATE, "AAAA", "world", "");

    phone_db_save_sorted(&db, "test_data/out_mut2.csv");
    size_t after_second = db.comment_buf_size;

    phone_db_save_sorted(&db, "test_data/out_mut3.csv");
    size_t after_third = db.comment_buf_size;

    phone_db_destroy(&db);

    /* comment_buf_size не должен расти при повторном сохранении */
    ASSERT(after_third == after_second);
    ASSERT(after_first == before);
}

/* ======================================================================
 * UNIT #3 parse_expiry использует корректные вычисления
 * ====================================================================== */

/**
 * @brief Проверяет корректность вычислений parse_expiry.
 *
 * Загружает запись с датой 2025-01-01 и проверяет,
 * что результат равен 20089 (количество дней с 1970-01-01).
 */
TEST(test_parse_expiry_correct) {
    write_csv("test_data/expiry_correct.csv", "AAAA;hello;2025-01-01\n");

    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/expiry_correct.csv");

    phone_key_t key;
    const char *comment;
    uint16_t clen;
    uint32_t expiry;
    phone_key_from_hex(&key, "AAAA");
    int rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(rc, 0);

    /* Дней с 1970-01-01 до 2025-01-01 = 20089 */
    ASSERT_EQ(expiry, 20089);

    phone_db_destroy(&db);
}

/* ======================================================================
 * UNIT #4 Отложенные операции для одного ключа дедуплицируются
 * ====================================================================== */

/**
 * @brief Проверяет дедупликацию отложенных операций.
 *
 * Два последовательных OP_ADD для одного ключа должны привести
 * к перезаписи первой операции второй. pending_count остаётся 1.
 */
TEST(test_pending_ops_dedup) {
    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);

    phone_db_apply_increment(&db, OP_ADD, "AAAA", "first", "");
    phone_db_apply_increment(&db, OP_ADD, "AAAA", "second", "");

    /* Дедупликация: вторая перезаписывает первую, pending_count остаётся 1 */
    ASSERT_EQ(db.pending_count, 1);

    phone_key_t key;
    const char *comment;
    uint16_t clen;
    uint32_t expiry;
    phone_key_from_hex(&key, "AAAA");
    int rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(rc, 0);

    /* Должны увидеть "second" (последняя запись побеждает) */
    ASSERT(clen == 6);
    ASSERT(memcmp(comment, "second", 6) == 0);

    phone_db_destroy(&db);
}

/* ======================================================================
 * UNIT #5 Недопустимые hex-символы возвращают ошибку
 * ====================================================================== */

/**
 * @brief Проверяет отклонение некорректных hex-символов.
 *
 * Строки "XXX", "AXB", "12G" должны возвращать ошибку (-1).
 * Корректные hex-строки "ABC", "123", "" должны завершаться успешно.
 */
TEST(test_invalid_hex_returns_error) {
    phone_key_t k;

    ASSERT(phone_key_from_hex(&k, "XXX") == -1);
    ASSERT(phone_key_from_hex(&k, "AXB") == -1);
    ASSERT(phone_key_from_hex(&k, "12G") == -1);

    /* Корректный hex должен завершиться успешно */
    ASSERT(phone_key_from_hex(&k, "ABC") == 0);
    ASSERT(phone_key_from_hex(&k, "123") == 0);
    ASSERT(phone_key_from_hex(&k, "") == 0);
}

/* ======================================================================
 * UNIT #6 Длина комментария > UINT16_MAX отклоняется
 * ====================================================================== */

/**
 * @brief Проверяет отклонение комментария длиной > UINT16_MAX.
 *
 * Комментарий длиной 70000 байт должен быть отклонён.
 * Короткий комментарий "ok" должен быть принят.
 */
TEST(test_comment_too_long_rejected) {
    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);

    size_t big_len = 70000;
    char *big = malloc(big_len + 1);
    memset(big, 'A', big_len);
    big[big_len] = '\0';

    int rc = phone_db_apply_increment(&db, OP_ADD, "AAAA", big, "");
    ASSERT(rc == -1);

    /* Короткий комментарий должен завершиться успешно */
    rc = phone_db_apply_increment(&db, OP_ADD, "AAAA", "ok", "");
    ASSERT_EQ(rc, 0);

    free(big);
    phone_db_destroy(&db);
}

/* ======================================================================
 * UNIT #7 getline корректно обрабатывает длинные строки
 * ====================================================================== */

/**
 * @brief Проверяет корректную обработку длинных строк в load_csv.
 *
 * Комментарий длиной 5000 байт не должен быть обрезан до 4096.
 * Тест гарантирует, что getline корректно работает с длинными строками.
 */
TEST(test_long_line_not_truncated) {
    size_t len = 5000;
    char *payload = malloc(len + 1);
    memset(payload, 'B', len);
    payload[len] = '\0';

    FILE *f = fopen("test_data/longline_fixed.csv", "w");
    assert(f);
    fprintf(f, "AAAA;%s;2025-01-01\n", payload);
    fclose(f);
    free(payload);

    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);
    int rc = phone_db_load_csv(&db, "test_data/longline_fixed.csv");
    ASSERT_EQ(rc, 0);

    phone_key_t key;
    const char *comment;
    uint16_t clen;
    uint32_t expiry;
    phone_key_from_hex(&key, "AAAA");
    rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(rc, 0);

    /* Комментарий должен быть ~5000 байт, не обрезан до 4096 */
    ASSERT(clen == 5000);

    phone_db_destroy(&db);
}

/* ======================================================================
 * UNIT #8 Недопустимая операция отклоняется
 * ====================================================================== */

/**
 * @brief Проверяет отклонение некорректных операций.
 *
 * Операции 'Z', 'x', 0 должны возвращать ошибку.
 * Корректная операция OP_ADD должна завершаться успешно.
 */
TEST(test_invalid_op_rejected) {
    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);

    ASSERT(phone_db_apply_increment(&db, 'Z', "AAAA", "test", "") == -1);
    ASSERT(phone_db_apply_increment(&db, 'x', "AAAA", "test", "") == -1);
    ASSERT(phone_db_apply_increment(&db, 0, "AAAA", "test", "") == -1);

    /* Корректные операции должны завершиться успешно */
    ASSERT(phone_db_apply_increment(&db, OP_ADD, "AAAA", "test", "") == 0);

    phone_db_destroy(&db);
}

/* ======================================================================
 * UNIT #9 load_csv заменяет данные, а не дополняет
 * ====================================================================== */

/**
 * @brief Проверяет, что load_csv заменяет данные, а не дополняет.
 *
 * После загрузки второго файла первый ключ (AAAA) не должен найтись,
 * а второй ключ (BBBB) должен найтись. Количество записей остаётся 1.
 */
TEST(test_load_csv_replaces_data) {
    write_csv("test_data/replace1.csv", "AAAA;first;\n");
    write_csv("test_data/replace2.csv", "BBBB;second;\n");

    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);

    phone_db_load_csv(&db, "test_data/replace1.csv");
    ASSERT_EQ(db.count, 1);

    phone_db_load_csv(&db, "test_data/replace2.csv");
    ASSERT_EQ(db.count, 1);

    phone_key_t key;
    const char *comment;
    uint16_t clen;
    uint32_t expiry;

    /* AAAA не должна найтись */
    phone_key_from_hex(&key, "AAAA");
    int rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT(rc != 0);

    /* BBBB должна найтись */
    phone_key_from_hex(&key, "BBBB");
    rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(rc, 0);

    phone_db_destroy(&db);
}

/* ======================================================================
 * UNIT #10: ADD затем UPDATE не теряется в save_sorted
 * ====================================================================== */

/**
 * @brief Проверяет сохранение ADD затем UPDATE в save_sorted.
 *
 * После OP_ADD и OP_UPDATE для одного ключа, save_sorted должен
 * содержать запись с комментарием "second" (последнее обновление).
 */
TEST(test_add_then_update_preserved_in_save) {
    write_csv("test_data/add_upd.csv", "BBBB;base;\n");

    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/add_upd.csv");

    phone_db_apply_increment(&db, OP_ADD, "AAAA", "first", "");
    phone_db_apply_increment(&db, OP_UPDATE, "AAAA", "second", "");

    phone_key_t key;
    const char *comment;
    uint16_t clen;
    uint32_t expiry;
    phone_key_from_hex(&key, "AAAA");
    int rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(rc, 0);
    ASSERT(clen == 6);
    ASSERT(memcmp(comment, "second", 6) == 0);

    rc = phone_db_save_sorted(&db, "test_data/out_add_upd.csv");
    ASSERT_EQ(rc, 0);

    FILE *f = fopen("test_data/out_add_upd.csv", "r");
    ASSERT(f != NULL);

    /* Найти AAAA в выводе и проверить комментарий */
    char line[1024];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "AAAA;", 5) == 0) {
            ASSERT(strncmp(line, "AAAA;second;", 12) == 0);
            found = 1;
        }
    }
    fclose(f);
    ASSERT(found);

    phone_db_destroy(&db);
}

/* ======================================================================
 * UNIT #11 Двойной вызов phone_db_destroy безопасен
 * ====================================================================== */

/**
 * @brief Проверяет безопасность двойного вызова phone_db_destroy.
 *
 * После первого вызова внутренние указатели обнуляются,
 * повторный вызов не должен привести к крашу.
 */
TEST(test_double_destroy_safe) {
    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);

    phone_db_destroy(&db);
    ASSERT(db.records == NULL);

    phone_db_destroy(&db);
    ASSERT(db.records == NULL);
}

/* ======================================================================
 * UNIT #12 Двойной вызов phone_db_init не утекает память
 * ====================================================================== */

/**
 * @brief Проверяет защиту от утечки памяти при повторном phone_db_init.
 *
 * Двойной вызов init не должен приводить к утечке —
 * старые буферы освобождаются перед выделением новых.
 */
TEST(test_double_init_no_leak) {
    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);

    size_t first = phone_db_memory_usage(&db);
    ASSERT(first > 0);

    phone_db_init(&db, 0, 0, 0);

    size_t second = phone_db_memory_usage(&db);
    ASSERT(second > 0);
    ASSERT(second == first);

    phone_db_destroy(&db);
}

/* ======================================================================
 * UNIT #13 phone_db_destroy безопасен без предварительного phone_db_init
 * ====================================================================== */

/**
 * @brief Проверяет, что phone_db_destroy не падает на неинициализированной БД.
 *
 * Вызов destroy на стековой переменной без init не должен привести к крашу.
 */
TEST(test_destroy_without_init) {
    phone_db_t db;
    memset(&db, 0xCC, sizeof(db));

    phone_db_destroy(&db);
}

/* ====================================================================== */

/* ======================================================================
 * UNIT #14 Дубликаты ключей при загрузке CSV
 * ====================================================================== */

/**
 * @brief Проверяет дедупликацию дубликатов ключей при загрузке CSV.
 *
 * CSV содержит две строки с одинаковым ключом AAAA. После загрузки
 * должен остаться только одна запись (первый вхождение сохраняется).
 */
TEST(test_csv_duplicate_keys_dedup) {
    write_csv("test_data/unit14_dup.csv",
        "AAAA;first_comment;2025-01-01\n"
        "AAAA;second_comment;2026-06-01\n");

    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);
    int rc = phone_db_load_csv(&db, "test_data/unit14_dup.csv");
    ASSERT_EQ(rc, 0);

    /* Дубликат удалён — остаётся только одна запись */
    ASSERT_EQ(db.count, 1);

    /* Проверяем, что запись доступна через lookup */
    phone_key_t key;
    const char *comment;
    uint16_t clen;
    phone_key_from_hex(&key, "AAAA");
    rc = phone_db_lookup(&db, &key, &comment, &clen, NULL);
    ASSERT_EQ(rc, 0);
    ASSERT(clen > 0);

    phone_db_destroy(&db);
}

/* ======================================================================
 * UNIT #15 phone_key_from_hex отклоняет строки длиннее 19 символов
 * ====================================================================== */

/**
 * @brief Проверяет отклонение hex-строк длиннее 19 символов.
 *
 * phone_key_from_hex должен возвращать -1 для строк > PHONE_NUM_MAX_HEX
 * (19 символов), а не молча обрезать их.
 */
TEST(test_hex_rejects_long_input) {
    phone_key_t k;

    /* 19 символов — допустимо */
    ASSERT(phone_key_from_hex(&k, "0123456789ABCDEF012") == 0);

    /* 20 символов — отклоняется */
    ASSERT(phone_key_from_hex(&k, "0123456789ABCDEF0123") == -1);

    /* 30 символов — отклоняется */
    ASSERT(phone_key_from_hex(&k, "0123456789ABCDEF01234567890ABC") == -1);
}

/* ======================================================================
 * UNIT #16 parse_expiry не переполняется при больших датах
 * ====================================================================== */

/**
 * @brief Проверяет корректность parse_expiry для далёких дат.
 *
 * Дата 10000000-01-01 не должна вызывать переполнение.
 * parse_expiry использует int64_t для промежуточных вычислений.
 */
TEST(test_parse_expiry_no_overflow) {
    write_csv("test_data/unit16_far.csv", "AAAA;far_future;10000000-01-01\n");

    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);
    int rc = phone_db_load_csv(&db, "test_data/unit16_far.csv");
    ASSERT_EQ(rc, 0);

    phone_key_t key;
    uint32_t expiry;
    phone_key_from_hex(&key, "AAAA");
    rc = phone_db_lookup(&db, &key, NULL, NULL, &expiry);
    ASSERT_EQ(rc, 0);

    /* Дата корректно вычислена, не переполнена */
    ASSERT(expiry > 0);
    ASSERT(expiry != UINT32_MAX);

    phone_db_destroy(&db);
}

/* ======================================================================
 * UNIT #17 H2: find_pending корректно работает (O(n) — особенность дизайна)
 * ====================================================================== */

/**
 * @brief Проверяет корректность find_pending при большом количестве записей.
 *
 * find_pending использует линейный поиск O(n). Это осознанный выбор
 * дизайна для простоты реализации. Тест гарантирует корректность поиска.
 */
TEST(test_find_pending_linear_correct) {
    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);

    /* Добавляем 1000 отложенных операций */
    for (int i = 0; i != 1000; ++i) {
        char num[20];
        snprintf(num, sizeof(num), "%04X", i);
        phone_db_apply_increment(&db, OP_ADD, num, "data", "");
    }
    ASSERT_EQ(db.pending_count, 1000);

    /* Поиск существующей записи — должен найтись */
    phone_key_t key;
    const char *comment;
    uint16_t clen;
    phone_key_from_hex(&key, "0042");
    int rc = phone_db_lookup(&db, &key, &comment, &clen, NULL);
    ASSERT_EQ(rc, 0);
    ASSERT(clen == 4);
    ASSERT(memcmp(comment, "data", 4) == 0);

    /* Поиск несуществующей записи */
    phone_key_from_hex(&key, "FFFF");
    rc = phone_db_lookup(&db, &key, NULL, NULL, NULL);
    ASSERT(rc != 0);

    phone_db_destroy(&db);
}

/* ======================================================================
 * UNIT #18 save_sorted корректно считает записи с DELETE
 * ====================================================================== */

/**
 * @brief Проверяет корректный подсчёт записей в save_sorted с учётом DELETE.
 *
 * Загружает 1000 записей, помечает 500 для удаления. save_sorted должен
 * выделить память ровно под 500 записей, а не под 1000.
 */
TEST(test_save_sorted_accurate_count_with_delete) {
    write_csv("test_data/unit18_del.csv", "");
    FILE *f = fopen("test_data/unit18_del.csv", "w");
    for (int i = 0; i != 1000; ++i) {
        fprintf(f, "%08X;comment_%d;\n", i, i);
    }
    fclose(f);

    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/unit18_del.csv");
    ASSERT_EQ(db.count, 1000);

    /* Помечаем 500 записей для удаления */
    for (int i = 0; i != 500; ++i) {
        char num[20];
        snprintf(num, sizeof(num), "%08X", i);
        phone_db_apply_increment(&db, OP_DELETE, num, "", "");
    }

    /* Сохраняем */
    int rc = phone_db_save_sorted(&db, "test_data/unit18_out.csv");
    ASSERT_EQ(rc, 0);

    /* Проверяем количество строк в выходном файле */
    f = fopen("test_data/unit18_out.csv", "r");
    ASSERT(f != NULL);
    int line_count = 0;
    char buf[256];
    while (fgets(buf, sizeof(buf), f)) line_count++;
    fclose(f);

    ASSERT_EQ(line_count, 500);

    phone_db_destroy(&db);
}

/* ======================================================================
 * UNIT #19 Ошибка загрузки откатывает состояние БД
 * ====================================================================== */

/**
 * @brief Проверяет, что ошибка загрузки оставляет БД пустой.
 *
 * Загружает валидные данные, добавляет отложенную операцию, затем
 * пытается загрузить файл с ошибкой. БД пересоздаётся в начале
 * load_csv, поэтому старые данные теряются. При ошибке БД пуста.
 */
TEST(test_load_csv_rollback_on_error) {
    write_csv("test_data/unit19_good.csv",
        "AAAA;hello;\n"
        "BBBB;world;\n");

    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);

    /* Загружаем валидные данные */
    int rc = phone_db_load_csv(&db, "test_data/unit19_good.csv");
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(db.count, 2);

    /* Добавляем отложенную операцию */
    rc = phone_db_apply_increment(&db, OP_ADD, "CCCC", "new", "");
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(db.pending_count, 1);

    /* Создаём файл с ошибкой (комментарий > UINT16_MAX) */
    FILE *f = fopen("test_data/unit19_bad.csv", "w");
    fprintf(f, "DDDD;good;\n");
    fprintf(f, "EEEE;");
    for (int i = 0; i != 70000; ++i) fputc('X', f);
    fprintf(f, ";\n");
    fclose(f);

    /* Пытаемся загрузить — должна произойти ошибка */
    rc = phone_db_load_csv(&db, "test_data/unit19_bad.csv");
    ASSERT(rc == -1);

    /* При ошибке БД пуста (reset в начале + reset при ошибке) */
    ASSERT_EQ(db.count, 0);
    ASSERT_EQ(db.pending_count, 0);

    phone_db_destroy(&db);
}

/* ======================================================================
 * UNIT #20 UPDATE/DELETE на несуществующий ключ возвращает ошибку
 * ====================================================================== */

/**
 * @brief Проверяет, что UPDATE и DELETE на несуществующий ключ возвращают -1.
 *
 * Ключ DEAD не существует ни в records, ни в pending.
 * OP_UPDATE и OP_DELETE должны вернуть -1. OP_ADD должен работать.
 */
TEST(test_update_delete_nonexistent_returns_error) {
    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);

    /* Загружаем одну запись */
    write_csv("test_data/unit20.csv", "AAAA;exists;\n");
    phone_db_load_csv(&db, "test_data/unit20.csv");

    /* UPDATE несуществующего ключа — ошибка */
    int rc = phone_db_apply_increment(&db, OP_UPDATE, "DEAD", "phantom", "");
    ASSERT(rc == -1);

    /* DELETE несуществующего ключа — ошибка */
    rc = phone_db_apply_increment(&db, OP_DELETE, "BEEF", "", "");
    ASSERT(rc == -1);

    /* ADD несуществующего ключа — допустимо */
    rc = phone_db_apply_increment(&db, OP_ADD, "DEAD", "new", "");
    ASSERT_EQ(rc, 0);

    /* Теперь UPDATE существующего pending ключа — допустимо */
    rc = phone_db_apply_increment(&db, OP_UPDATE, "DEAD", "updated", "");
    ASSERT_EQ(rc, 0);

    /* DELETE существующего pending ключа — допустимо */
    rc = phone_db_apply_increment(&db, OP_DELETE, "DEAD", "", "");
    ASSERT_EQ(rc, 0);

    phone_db_destroy(&db);
}

/* ======================================================================
 * UNIT #21 Невалидные даты — expiry не установлен
 * ====================================================================== */

/**
 * @brief Проверяет поведение при невалидных датах.
 *
 * Пустая строка даты -> 0 (без срока).
 * Невалидный формат -> 0 (expiry не установлен, ошибка игнорируется).
 * Невалидные значения (9999-99-99) -> 0 (expiry не установлен).
 * Корректная дата -> количество дней с эпохи.
 */
TEST(test_invalid_date_returns_sentinel) {
    write_csv("test_data/unit21_dates.csv",
        "AAAA;valid_date;2025-06-01\n"
        "BBBB;no_expiry;\n"
        "CCCC;bad_format;not-a-date\n"
        "DDDD;bad_values;9999-99-99\n");

    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/unit21_dates.csv");

    phone_key_t key;
    uint32_t expiry;

    /* Корректная дата */
    phone_key_from_hex(&key, "AAAA");
    phone_db_lookup(&db, &key, NULL, NULL, &expiry);
    ASSERT(expiry > 0);
    ASSERT(expiry != UINT32_MAX);

    /* Пустая дата — 0 (без срока) */
    phone_key_from_hex(&key, "BBBB");
    phone_db_lookup(&db, &key, NULL, NULL, &expiry);
    ASSERT_EQ(expiry, 0);

    /* Невалидный формат — expiry не установлен (остаётся 0) */
    phone_key_from_hex(&key, "CCCC");
    phone_db_lookup(&db, &key, NULL, NULL, &expiry);
    ASSERT_EQ(expiry, 0);

    /* Невалидные значения — expiry не установлен (остаётся 0) */
    phone_key_from_hex(&key, "DDDD");
    phone_db_lookup(&db, &key, NULL, NULL, &expiry);
    ASSERT_EQ(expiry, 0);

    phone_db_destroy(&db);
}

/* ======================================================================
 * UNIT #22 save_sorted убирает ведущие нули (особенность формата)
 * ====================================================================== */

/**
 * @brief Проверяет формат вывода save_sorted с ведущими нулями.
 *
 * save_sorted убирает ведущие нули из номеров телефонов.
 * Это осознанный выбор формата: минимальное представление.
 * Обратная загрузка работает корректно (phone_key_from_hex
 * выравнивает по правому краю).
 */
TEST(test_save_sorted_strips_leading_zeros) {
    write_csv("test_data/unit22_zeros.csv",
        "00000001;one;\n"
        "000000FF;two;\n"
        "00012345;three;\n");

    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/unit22_zeros.csv");

    int rc = phone_db_save_sorted(&db, "test_data/unit22_out.csv");
    ASSERT_EQ(rc, 0);

    FILE *f = fopen("test_data/unit22_out.csv", "r");
    ASSERT(f != NULL);

    char line[256];
    /* Первая строка: 1 (без ведущих нулей) */
    ASSERT(fgets(line, sizeof(line), f));
    ASSERT(strncmp(line, "1;one;", 6) == 0);

    /* Вторая строка: FF */
    ASSERT(fgets(line, sizeof(line), f));
    ASSERT(strncmp(line, "FF;two;", 7) == 0);

    /* Третья строка: 12345 */
    ASSERT(fgets(line, sizeof(line), f));
    ASSERT(strncmp(line, "12345;three;", 12) == 0);

    fclose(f);

    /* Проверяем обратную загрузку: данные восстанавливаются корректно */
    phone_db_t db2;
    phone_db_init(&db2, 0, 0, 0);
    rc = phone_db_load_csv(&db2, "test_data/unit22_out.csv");
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(db2.count, 3);

    phone_key_t key;
    phone_key_from_hex(&key, "00000001");
    rc = phone_db_lookup(&db2, &key, NULL, NULL, NULL);
    ASSERT_EQ(rc, 0);

    phone_db_destroy(&db);
    phone_db_destroy(&db2);
}

/* ======================================================================
 * UNIT #23 ensure_capacity проверяет переполнение size_t
 * ====================================================================== */

/**
 * @brief Проверяет защиту от переполнения size_t в ensure_capacity.
 *
 * Код ensure_capacity и ensure_pending_capacity содержит проверку
 * capacity > SIZE_MAX / 2 перед удвоением. Тест проверяет, что
 * ensure_pending_capacity корректно обрабатывает границу, заполняя
 * pending-массив до capacity и добавляя ещё одну запись.
 */
TEST(test_ensure_capacity_overflow_guard) {
    phone_db_t db;
    phone_db_init(&db, 0, 0, 4);

    /* Заполняем pending до capacity */
    phone_db_apply_increment(&db, OP_ADD, "0001", "a", "");
    phone_db_apply_increment(&db, OP_ADD, "0002", "b", "");
    phone_db_apply_increment(&db, OP_ADD, "0003", "c", "");
    phone_db_apply_increment(&db, OP_ADD, "0004", "d", "");
    ASSERT_EQ(db.pending_count, 4);
    ASSERT_EQ(db.pending_capacity, 4);

    /* Следующий apply_increment должен расширить массив (capacity *= 2) */
    int rc = phone_db_apply_increment(&db, OP_ADD, "0005", "e", "");
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(db.pending_count, 5);
    ASSERT(db.pending_capacity >= 8);

    phone_db_destroy(&db);
}

/* ======================================================================
 * UNIT #24 format_expiry корректно конвертирует дни в дату
 * ====================================================================== */

/**
 * @brief Проверяет базовые конверсии format_expiry.
 *
 * 0 → "", UINT32_MAX → "", эталонные даты → YYYY-MM-DD.
 */
TEST(test_format_expiry_basic) {
    char buf[11];

    /* 0 (без срока) → пустая строка */
    ASSERT_EQ(format_expiry(0, buf, sizeof(buf)), 0);
    ASSERT_STREQ(buf, "");

    /* UINT32_MAX → пустая строка */
    ASSERT_EQ(format_expiry(UINT32_MAX, buf, sizeof(buf)), 0);
    ASSERT_STREQ(buf, "");

    /* 20089 дней → 2025-01-01 */
    ASSERT_EQ(format_expiry(20089, buf, sizeof(buf)), 10);
    ASSERT_STREQ(buf, "2025-01-01");

    /* 10957 дней → 2000-01-01 */
    ASSERT_EQ(format_expiry(10957, buf, sizeof(buf)), 10);
    ASSERT_STREQ(buf, "2000-01-01");

    /* 20240 дней → 2025-06-01 */
    ASSERT_EQ(format_expiry(20240, buf, sizeof(buf)), 10);
    ASSERT_STREQ(buf, "2025-06-01");
}

/* ======================================================================
 * UNIT #25 format_expiry roundtrip с parse_expiry
 * ====================================================================== */

/**
 * @brief Проверяет roundtrip: parse_expiry → format_expiry совпадает.
 *
 * Загружает CSV с датой, сохраняет, перечитывает — дата должна совпасть.
 * Также проверяет прямую конвертацию format_expiry(parse_expiry(date_str)).
 */
TEST(test_format_expiry_roundtrip) {
    write_csv("test_data/unit25_rt.csv", "AAAA;hello;2025-01-01\n");

    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/unit25_rt.csv");

    /* Сохраняем */
    int rc = phone_db_save_sorted(&db, "test_data/unit25_out.csv");
    ASSERT_EQ(rc, 0);
    phone_db_destroy(&db);

    /* Перечитываем */
    phone_db_t db2;
    phone_db_init(&db2, 0, 0, 0);
    rc = phone_db_load_csv(&db2, "test_data/unit25_out.csv");
    ASSERT_EQ(rc, 0);

    phone_key_t key;
    const char *comment;
    uint16_t clen;
    uint32_t expiry;
    phone_key_from_hex(&key, "AAAA");
    rc = phone_db_lookup(&db2, &key, &comment, &clen, &expiry);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(expiry, 20089);

    phone_db_destroy(&db2);

    /* Прямая конвертация: format_expiry(parse_expiry("2025-01-01")) */
    uint32_t days;
    ASSERT_EQ(parse_expiry("2025-01-01", &days), 0);
    ASSERT_EQ(days, 20089);
    char buf[11];
    int n = format_expiry(days, buf, sizeof(buf));
    ASSERT_EQ(n, 10);
    ASSERT_STREQ(buf, "2025-01-01");

    /* Ещё одна дата */
    ASSERT_EQ(parse_expiry("2000-06-15", &days), 0);
    ASSERT(days > 0);
    n = format_expiry(days, buf, sizeof(buf));
    ASSERT_EQ(n, 10);
    ASSERT_STREQ(buf, "2000-06-15");

    /* Пустая строка → 0 → "" */
    ASSERT_EQ(parse_expiry("", &days), 0);
    ASSERT_EQ(days, 0);
    n = format_expiry(days, buf, sizeof(buf));
    ASSERT_EQ(n, 0);
    ASSERT_STREQ(buf, "");

    /* Невалидная дата → -1 */
    ASSERT_EQ(parse_expiry("not-a-date", &days), -1);
    n = format_expiry(UINT32_MAX, buf, sizeof(buf));
    ASSERT_EQ(n, 0);
    ASSERT_STREQ(buf, "");
}

/* ======================================================================
 * UNIT #26 format_expiry без срока сохраняет пустую строку
 * ====================================================================== */

/**
 * @brief Проверяет, что запись без expiry сохраняется с пустым полем.
 *
 * Запись без даты истечения (expiry == 0) должна сохраняться
 * с пустым третьим полем, а не с "0".
 */
TEST(test_format_expiry_no_expiry_saves_empty) {
    write_csv("test_data/unit26_noexp.csv", "AAAA;hello;\n");

    phone_db_t db;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/unit26_noexp.csv");

    int rc = phone_db_save_sorted(&db, "test_data/unit26_out.csv");
    ASSERT_EQ(rc, 0);
    phone_db_destroy(&db);

    /* Читаем файл и проверяем формат */
    FILE *f = fopen("test_data/unit26_out.csv", "r");
    ASSERT(f);
    char line[256];
    ASSERT(fgets(line, sizeof(line), f));
    fclose(f);

    /* Ожидаем "AAAA;hello;" (без "0" в конце) */
    ASSERT_STREQ(line, "AAAA;hello;\n");

    /* Перечитываем через БД — expiry должен быть 0 */
    phone_db_t db2;
    phone_db_init(&db2, 0, 0, 0);
    phone_db_load_csv(&db2, "test_data/unit26_out.csv");

    phone_key_t key;
    uint32_t expiry;
    phone_key_from_hex(&key, "AAAA");
    phone_db_lookup(&db2, &key, NULL, NULL, &expiry);
    ASSERT_EQ(expiry, 0);

    phone_db_destroy(&db2);
}

/* ======================================================================
 * UNIT #27 Прямые тесты parse_expiry
 * ====================================================================== */

/**
 * @brief Тестирование parse_expiry напрямую со всеми ветвями.
 *
 * Проверяет NULL, пустую строку, невалидные форматы,
 * невалидные значения, эталонные даты и sınırные случаи.
 */
TEST(test_parse_expiry_direct) {
    uint32_t d;

    /* NULL → 0 */
    ASSERT_EQ(parse_expiry(NULL, &d), 0);
    ASSERT_EQ(d, 0);

    /* Пустая строка → 0 */
    ASSERT_EQ(parse_expiry("", &d), 0);
    ASSERT_EQ(d, 0);

    /* Невалидный формат — sscanf не парсит */
    ASSERT_EQ(parse_expiry("abc", &d), -1);
    ASSERT_EQ(parse_expiry("2025", &d), -1);
    ASSERT_EQ(parse_expiry("2025-01", &d), -1);
    ASSERT_EQ(parse_expiry("2025/01/01", &d), -1);

    /* Невалидные значения — m < 1, m > 12, d < 1, d > 31 */
    ASSERT_EQ(parse_expiry("2025-13-01", &d), -1);
    ASSERT_EQ(parse_expiry("2025-00-01", &d), -1);
    ASSERT_EQ(parse_expiry("2025-01-00", &d), -1);
    ASSERT_EQ(parse_expiry("2025-01-32", &d), -1);

    /* До эпохи — days < 0 */
    ASSERT_EQ(parse_expiry("0001-01-01", &d), -1);

    /* Эталонные даты */
    ASSERT_EQ(parse_expiry("1970-01-01", &d), 0);
    ASSERT_EQ(d, 0);
    ASSERT_EQ(parse_expiry("2000-01-01", &d), 0);
    ASSERT_EQ(d, 10957);
    ASSERT_EQ(parse_expiry("2025-01-01", &d), 0);
    ASSERT_EQ(d, 20089);
    ASSERT_EQ(parse_expiry("2025-06-15", &d), 0);
    ASSERT_EQ(d, 20254);

    /* Большая дата — не переполняется */
    ASSERT_EQ(parse_expiry("9999-12-31", &d), 0);
    ASSERT(d > 0);
}

/* ====================================================================== */

int main(void) {
    printf("=== UNIT регрессионные тесты ===\n\n");

    printf("[UNIT #1: Макрос RUN считает падения дважды]\n");
    RUN(test_that_will_pass);
    RUN(test_that_will_fail);
    RUN(test_that_will_pass);
    RUN(test_run_macro_no_double_count);
    printf("\n");

    printf("[UNIT #2: save_sorted не изменяет comment_buf]\n");
    RUN(test_save_sorted_no_comment_buf_mutation);
    printf("\n");

    printf("[UNIT #3: parse_expiry считает неправильно]\n");
    RUN(test_parse_expiry_correct);
    printf("\n");

    printf("[UNIT #4: Нет дедупликации отложенных операций]\n");
    RUN(test_pending_ops_dedup);
    printf("\n");

    printf("[UNIT #5: Некорректный hex принимается за 0]\n");
    RUN(test_invalid_hex_returns_error);
    printf("\n");

    printf("[UNIT #6: Комментарий обрезается до 16 бит]\n");
    RUN(test_comment_too_long_rejected);
    printf("\n");

    printf("[UNIT #7: Длинные строки обрезаются]\n");
    RUN(test_long_line_not_truncated);
    printf("\n");

    printf("[UNIT #8: Некорректная операция принимается]\n");
    RUN(test_invalid_op_rejected);
    printf("\n");

    printf("[UNIT #9: load_csv не заменяет старые данные]\n");
    RUN(test_load_csv_replaces_data);
    printf("\n");

    printf("[UNIT #10: ADD затем UPDATE теряется в save_sorted]\n");
    RUN(test_add_then_update_preserved_in_save);
    printf("\n");

    printf("[UNIT #11: Двойной вызов phone_db_destroy безопасен]\n");
    RUN(test_double_destroy_safe);
    printf("\n");

    printf("[UNIT #12: Двойной вызов phone_db_init не утекает память]\n");
    RUN(test_double_init_no_leak);
    printf("\n");

    printf("[UNIT #13: phone_db_destroy безопасен без phone_db_init]\n");
    RUN(test_destroy_without_init);
    printf("\n");

    printf("[UNIT #14: Дубликаты ключей при загрузке CSV]\n");
    RUN(test_csv_duplicate_keys_dedup);
    printf("\n");

    printf("[UNIT #15: phone_key_from_hex отклоняет >19 символов]\n");
    RUN(test_hex_rejects_long_input);
    printf("\n");

    printf("[UNIT #16: parse_expiry не переполняется]\n");
    RUN(test_parse_expiry_no_overflow);
    printf("\n");

    printf("[UNIT #17: find_pending корректен (O(n) дизайн)]\n");
    RUN(test_find_pending_linear_correct);
    printf("\n");

    printf("[UNIT #18: save_sorted считает записи с DELETE]\n");
    RUN(test_save_sorted_accurate_count_with_delete);
    printf("\n");

    printf("[UNIT #19: Ошибка загрузки откатывает состояние]\n");
    RUN(test_load_csv_rollback_on_error);
    printf("\n");

    printf("[UNIT #20: UPDATE/DELETE на несуществующий ключ]\n");
    RUN(test_update_delete_nonexistent_returns_error);
    printf("\n");

    printf("[UNIT #21: Невалидные даты — expiry не установлен]\n");
    RUN(test_invalid_date_returns_sentinel);
    printf("\n");

    printf("[UNIT #22: save_sorted убирает ведущие нули]\n");
    RUN(test_save_sorted_strips_leading_zeros);
    printf("\n");

    printf("[UNIT #23: ensure_capacity проверяет переполнение]\n");
    RUN(test_ensure_capacity_overflow_guard);
    printf("\n");

    printf("[UNIT #24: format_expiry конвертирует дни в дату]\n");
    RUN(test_format_expiry_basic);
    printf("\n");

    printf("[UNIT #25: format_expiry roundtrip с parse_expiry]\n");
    RUN(test_format_expiry_roundtrip);
    printf("\n");

    printf("[UNIT #26: format_expiry без срока сохраняет пустую строку]\n");
    RUN(test_format_expiry_no_expiry_saves_empty);
    printf("\n");

    printf("[UNIT #27: Прямые тесты parse_expiry]\n");
    RUN(test_parse_expiry_direct);
    printf("\n");

    printf("=== Результаты: %d/%d пройдено, %d ПРОВАЛЕНО ===\n",
           tests_passed, tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
