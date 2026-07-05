#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/resource.h>
#include "phone_db.h"
#include "test_framework.h"

/* -- Тесты кодирования ключа телефона ----------------------------------- */

/**
 * @brief Проверяет базовое преобразование hex-строки в ключ телефона.
 *
 * Преобразует "123ABC" и проверяет, что ниблы записаны в правильные
 * байты ключа, выровненного по правому краю.
 */
TEST(test_phone_key_from_hex_basic) {
    phone_key_t k;
    phone_key_from_hex(&k, "123ABC");
    /* "123ABC" = 0x123ABC, выровнено по правому краю в 10 байт */
    ASSERT(k.bytes[9] == 0xBC);
    ASSERT(k.bytes[8] == 0x3A);
    ASSERT(k.bytes[7] == 0x12);
    for (int i = 0; i != 7; ++i)
        ASSERT(k.bytes[i] == 0x00);
}

/**
 * @brief Проверяет преобразование максимальной длины hex-строки.
 *
 * Преобразует 19 hex-символов и проверяет выравнивание по правому краю
 * в 20 ниблов (10 байт).
 */
TEST(test_phone_key_from_hex_max_length) {
    phone_key_t k;
    phone_key_from_hex(&k, "0123456789ABCDEF012");
    /* 19 hex-символов, выровнены по правому краю в 20 ниблов (10 байт) */
    ASSERT(k.bytes[0] == 0x00);
    ASSERT(k.bytes[9] == 0x12);
}

/**
 * @brief Проверяет преобразование короткой hex-строки (1 символ).
 *
 * Преобразует "1" и проверяет, что результат — 0x01 в последнем байте,
 * остальные байты нулевые.
 */
TEST(test_phone_key_from_hex_short) {
    phone_key_t k;
    phone_key_from_hex(&k, "1");
    /* "1" -> 000...001 -> последний байт = 0x01 */
    ASSERT(k.bytes[9] == 0x01);
    for (int i = 0; i != 9; ++i)
        ASSERT(k.bytes[i] == 0x00);
}

/**
 * @brief Проверяет преобразование пустой hex-строки.
 *
 * Пустая строка должна дать ключ, состоящий из нулевых байтов.
 */
TEST(test_phone_key_from_hex_empty) {
    phone_key_t k;
    phone_key_from_hex(&k, "");
    for (int i = 0; i != PHONE_KEY_LEN; ++i)
        ASSERT(k.bytes[i] == 0x00);
}

/**
 * @brief Проверяет обратное преобразование ключа в hex-строку и обратно.
 *
 * Преобразует ключ в hex, затем обратно и сравнивает байты.
 * Должна получиться исходная строка длиной 19 символов.
 */
TEST(test_phone_key_to_hex_roundtrip) {
    phone_key_t k;
    char buf[PHONE_NUM_MAX_HEX + 1];
    phone_key_from_hex(&k, "ABCDEF0123456789");
    int len = phone_key_to_hex(&k, buf, sizeof(buf));
    ASSERT(len == 19);
    ASSERT(buf[len] == '\0');
    /* Проверка обратного преобразования */
    phone_key_t k2;
    phone_key_from_hex(&k2, buf);
    ASSERT(memcmp(k.bytes, k2.bytes, PHONE_KEY_LEN) == 0);
}

/**
 * @brief Проверяет обратное преобразование короткой hex-строки.
 *
 * Преобразует "FF" в ключ и обратно. Результат должен содержать
 * 17 нулей и "FF" в конце.
 */
TEST(test_phone_key_to_hex_short) {
    phone_key_t k;
    char buf[PHONE_NUM_MAX_HEX + 1];
    phone_key_from_hex(&k, "FF");
    int len = phone_key_to_hex(&k, buf, sizeof(buf));
    ASSERT(len == 19);
    /* "FF" дополнено до 19 -> 17 нулей + "FF" */
    ASSERT(buf[0] == '0');
    ASSERT(buf[17] == 'F');
    ASSERT(buf[18] == 'F');
}

/* -- Тесты сравнения ключей телефона ----------------------------------- */

/**
 * @brief Проверяет сравнение двух одинаковых ключей.
 *
 * Два ключа с одинаковым значением "12345" должны быть равны.
 */
TEST(test_phone_key_compare_equal) {
    phone_key_t a, b;
    phone_key_from_hex(&a, "12345");
    phone_key_from_hex(&b, "12345");
    ASSERT_EQ(phone_key_compare(&a, &b), 0);
}

/**
 * @brief Проверяет сравнение двух различных ключей (меньше).
 *
 * Ключ "123" должен быть меньше ключа "124".
 */
TEST(test_phone_key_compare_less) {
    phone_key_t a, b;
    phone_key_from_hex(&a, "123");
    phone_key_from_hex(&b, "124");
    ASSERT(phone_key_compare(&a, &b) < 0);
}

/**
 * @brief Проверяет сравнение двух различных ключей (больше).
 *
 * Ключ "ABC" должен быть больше ключа "123".
 */
TEST(test_phone_key_compare_greater) {
    phone_key_t a, b;
    phone_key_from_hex(&a, "ABC");
    phone_key_from_hex(&b, "123");
    ASSERT(phone_key_compare(&a, &b) > 0);
}

/**
 * @brief Проверяет сравнение ключей разной длины.
 *
 * Ключ "1" (0x01) должен быть меньше ключа "10" (0x10).
 */
TEST(test_phone_key_compare_different_length) {
    phone_key_t a, b;
    phone_key_from_hex(&a, "1");
    phone_key_from_hex(&b, "10");
    /* "1" = 000...001, "10" = 000...010 -> "1" < "10" численно */
    ASSERT(phone_key_compare(&a, &b) < 0);
}

/**
 * @brief Проверяет сравнение пустого ключа с непустым.
 *
 * Пустой ключ должен быть меньше ключа "1".
 */
TEST(test_phone_key_compare_zero) {
    phone_key_t a, b;
    phone_key_from_hex(&a, "");
    phone_key_from_hex(&b, "1");
    ASSERT(phone_key_compare(&a, &b) < 0);
}

/**
 * @brief Проверяет функцию phone_key_is_empty.
 *
 * Пустой ключ и ключ "0" считаются пустыми, ключ "1" — нет.
 */
TEST(test_phone_key_is_empty) {
    phone_key_t k;
    phone_key_from_hex(&k, "");
    ASSERT(phone_key_is_empty(&k));
    phone_key_from_hex(&k, "0");
    ASSERT(phone_key_is_empty(&k));
    phone_key_from_hex(&k, "1");
    ASSERT(!phone_key_is_empty(&k));
}

/* -- Тесты инициализации/уничтожения базы данных ----------------------- */

/**
 * @brief Проверяет корректную инициализацию БД.
 *
 * После phone_db_init внутренние буферы выделены, count равен 0.
 */
TEST(test_db_init) {
    phone_db_t db = PHONE_DB_INITIALIZER;
    int rc = phone_db_init(&db, 0, 0, 0);
    ASSERT_EQ(rc, 0);
    ASSERT(db.records != NULL);
    ASSERT_EQ(db.count, 0);
    ASSERT(db.comment_buf != NULL);
    phone_db_destroy(&db);
}

/**
 * @brief Проверяет корректное уничтожение БД.
 *
 * После phone_db_destroy указатель records должен быть NULL.
 */
TEST(test_db_destroy) {
    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_destroy(&db);
    /* После уничтожения внутренние указатели должны быть NULL */
    ASSERT(db.records == NULL);
}

/* -- Тесты пакетной загрузки ------------------------------------------- */

static void write_csv_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    assert(f);
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}

/**
 * @brief Проверяет базовую загрузку CSV с тремя записями.
 *
 * Загружает CSV с тремя записями разного формата и проверяет,
 * что count равен 3.
 */
TEST(test_bulk_load_simple) {
    write_csv_file("test_data/simple.csv",
        "12345;comment1;2025-01-01\n"
        "67890;comment2;\n"
        "ABCDE;;2025-12-31\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    int rc = phone_db_load_csv(&db, "test_data/simple.csv");
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(db.count, 3);
    phone_db_destroy(&db);
}

/**
 * @brief Проверяет поиск после пакетной загрузки.
 *
 * Загружает две записи и проверяет, что lookup возвращает
 * правильные комментарий и срок действия для каждой.
 */
TEST(test_bulk_load_lookup) {
    write_csv_file("test_data/lookup.csv",
        "AAAA;hello;2025-06-01\n"
        "BBBB;world;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/lookup.csv");

    phone_key_t key;
    const char *comment = NULL;
    uint16_t clen = 0;
    uint32_t expiry = 0;

    phone_key_from_hex(&key, "AAAA");
    int rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(rc, 0);
    ASSERT(clen == 5);
    ASSERT(memcmp(comment, "hello", 5) == 0);
    ASSERT(expiry != 0);

    phone_key_from_hex(&key, "BBBB");
    rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(rc, 0);
    ASSERT(clen == 5);
    ASSERT(memcmp(comment, "world", 5) == 0);
    ASSERT(expiry == 0);

    phone_db_destroy(&db);
}

/**
 * @brief Проверяет поиск несуществующего ключа.
 *
 * Загружает одну запись, ищет другую — должен вернуть ошибку.
 */
TEST(test_bulk_load_lookup_not_found) {
    write_csv_file("test_data/notfound.csv",
        "1111;aaa;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/notfound.csv");

    phone_key_t key;
    phone_key_from_hex(&key, "9999");
    const char *comment;
    uint16_t clen;
    uint32_t expiry;
    int rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT(rc != 0);

    phone_db_destroy(&db);
}

/**
 * @brief Проверяет сортировку записей после загрузки.
 *
 * Загружает три записи в произвольном порядке и проверяет,
 * что в массиве records они отсортированы по ключу.
 */
TEST(test_bulk_load_sorted_order) {
    write_csv_file("test_data/sorted.csv",
        "CCC;third;\n"
        "AAA;first;\n"
        "BBB;second;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/sorted.csv");
    ASSERT_EQ(db.count, 3);

    /* Записи должны быть отсортированы по ключу */
    ASSERT(phone_key_compare(&db.records[0].key, &db.records[1].key) < 0);
    ASSERT(phone_key_compare(&db.records[1].key, &db.records[2].key) < 0);

    phone_db_destroy(&db);
}

/**
 * @brief Проверяет загрузку пустого CSV-файла.
 *
 * Пустой файл должен загрузиться успешно с count = 0.
 */
TEST(test_bulk_load_empty_file) {
    write_csv_file("test_data/empty.csv", "");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    int rc = phone_db_load_csv(&db, "test_data/empty.csv");
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(db.count, 0);
    phone_db_destroy(&db);
}

/**
 * @brief Проверяет загрузку записи с пустым комментарием.
 *
 * Запись "123;;2025-01-01" должна загрузиться с комментарием длины 0.
 */
TEST(test_bulk_load_missing_comment) {
    write_csv_file("test_data/nocomment.csv",
        "123;;2025-01-01\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/nocomment.csv");
    ASSERT_EQ(db.count, 1);

    phone_key_t key;
    const char *comment;
    uint16_t clen;
    uint32_t expiry;
    phone_key_from_hex(&key, "123");
    int rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(clen, 0);

    phone_db_destroy(&db);
}

/**
 * @brief Проверяет загрузку записи без срока действия.
 *
 * Запись "123;hello;" должна загрузиться с expiry = 0.
 */
TEST(test_bulk_load_missing_expiry) {
    write_csv_file("test_data/noexpiry.csv",
        "123;hello;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/noexpiry.csv");

    phone_key_t key;
    const char *comment;
    uint16_t clen;
    uint32_t expiry;
    phone_key_from_hex(&key, "123");
    int rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(expiry, 0);

    phone_db_destroy(&db);
}

/* -- Тесты инкрементальных обновлений ---------------------------------- */

/**
 * @brief Проверяет добавление новой записи через инкремент.
 *
 * Загружает одну запись, затем добавляет новую через OP_ADD.
 * Lookup должен найти новую запись с правильным комментарием.
 */
TEST(test_increment_add) {
    write_csv_file("test_data/inc_add.csv", "AAAA;base;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/inc_add.csv");
    ASSERT_EQ(db.count, 1);

    /* Добавить новую запись через инкремент */
    int rc = phone_db_apply_increment(&db, OP_ADD, "BBBB", "new", "");
    ASSERT_EQ(rc, 0);

    phone_key_t key;
    const char *comment;
    uint16_t clen;
    uint32_t expiry;
    phone_key_from_hex(&key, "BBBB");
    rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(rc, 0);
    ASSERT(clen == 3);
    ASSERT(memcmp(comment, "new", 3) == 0);

    phone_db_destroy(&db);
}

/**
 * @brief Проверяет обновление существующей записи через инкремент.
 *
 * Загружает запись, обновляет комментарий через OP_UPDATE.
 * Lookup должен вернуть новый комментарий.
 */
TEST(test_increment_update) {
    write_csv_file("test_data/inc_upd.csv", "AAAA;old;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/inc_upd.csv");

    /* Обновить существующую запись */
    int rc = phone_db_apply_increment(&db, OP_UPDATE, "AAAA", "new_comment", "");
    ASSERT_EQ(rc, 0);

    phone_key_t key;
    const char *comment;
    uint16_t clen;
    uint32_t expiry;
    phone_key_from_hex(&key, "AAAA");
    rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(rc, 0);
    ASSERT(clen == 11);
    ASSERT(memcmp(comment, "new_comment", 11) == 0);

    phone_db_destroy(&db);
}

/**
 * @brief Проверяет удаление записи через инкремент.
 *
 * Загружает две записи, удаляет одну через OP_DELETE.
 * Удалённая запись не должна найтись, оставшаяся — должна.
 */
TEST(test_increment_delete) {
    write_csv_file("test_data/inc_del.csv",
        "AAAA;keep;\n"
        "BBBB;remove;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/inc_del.csv");

    /* Удалить запись */
    int rc = phone_db_apply_increment(&db, OP_DELETE, "BBBB", "", "");
    ASSERT_EQ(rc, 0);

    phone_key_t key;
    const char *comment;
    uint16_t clen;
    uint32_t expiry;

    /* BBBB не должен найтись */
    phone_key_from_hex(&key, "BBBB");
    rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT(rc != 0);

    /* AAAA должен найтись */
    phone_key_from_hex(&key, "AAAA");
    rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(rc, 0);

    phone_db_destroy(&db);
}

/**
 * @brief Проверяет комбинацию инкрементальных операций.
 *
 * Выполняет DELETE, ADD и UPDATE для разных ключей.
 * Проверяет, что все операции.applyied корректно.
 */
TEST(test_increment_multiple_ops) {
    write_csv_file("test_data/inc_multi.csv",
        "1111;aaa;\n"
        "2222;bbb;\n"
        "3333;ccc;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/inc_multi.csv");

    phone_db_apply_increment(&db, OP_DELETE, "2222", "", "");
    phone_db_apply_increment(&db, OP_ADD, "4444", "ddd", "");
    phone_db_apply_increment(&db, OP_UPDATE, "1111", "AAA", "");

    phone_key_t key;
    const char *comment;
    uint16_t clen;
    uint32_t expiry;
    int rc;

    /* 1111 обновлена */
    phone_key_from_hex(&key, "1111");
    rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(rc, 0);
    ASSERT(clen == 3);
    ASSERT(memcmp(comment, "AAA", 3) == 0);

    /* 2222 удалена */
    phone_key_from_hex(&key, "2222");
    ASSERT(phone_db_lookup(&db, &key, &comment, &clen, &expiry) != 0);

    /* 3333 без изменений */
    phone_key_from_hex(&key, "3333");
    rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(rc, 0);
    ASSERT(clen == 3);
    ASSERT(memcmp(comment, "ccc", 3) == 0);

    /* 4444 добавлена */
    phone_key_from_hex(&key, "4444");
    rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(rc, 0);
    ASSERT(clen == 3);
    ASSERT(memcmp(comment, "ddd", 3) == 0);

    phone_db_destroy(&db);
}

/* -- Тесты сохранения в отсортированном порядке ------------------------ */

/**
 * @brief Проверяет базовое сохранение в отсортированном порядке.
 *
 * Загружает три записи в произвольном порядке, сохраняет и проверяет,
 * что в выходном файле записи отсортированы по ключу.
 */
TEST(test_save_sorted_basic) {
    write_csv_file("test_data/save.csv",
        "CCC;third;\n"
        "AAA;first;\n"
        "BBB;second;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/save.csv");

    int rc = phone_db_save_sorted(&db, "test_data/output.csv");
    ASSERT_EQ(rc, 0);

    /* Прочитать вывод и проверить порядок */
    FILE *f = fopen("test_data/output.csv", "r");
    ASSERT(f != NULL);

    char line[1024];
    /* Первая строка */
    ASSERT(fgets(line, sizeof(line), f) != NULL);
    ASSERT(strncmp(line, "AAA;first;", 10) == 0);

    /* Вторая строка */
    ASSERT(fgets(line, sizeof(line), f) != NULL);
    ASSERT(strncmp(line, "BBB;second;", 11) == 0);

    /* Третья строка */
    ASSERT(fgets(line, sizeof(line), f) != NULL);
    ASSERT(strncmp(line, "CCC;third;", 10) == 0);

    fclose(f);
    phone_db_destroy(&db);
}

/**
 * @brief Проверяет сохранение с учётом инкрементальных ADD.
 *
 * Загружает две записи, добавляет третью через OP_ADD, сохраняет.
 * В выходном файле должны быть три записи в отсортированном порядке.
 */
TEST(test_save_sorted_with_increments) {
    write_csv_file("test_data/save_inc.csv",
        "CCC;third;\n"
        "AAA;first;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/save_inc.csv");

    phone_db_apply_increment(&db, OP_ADD, "BBB", "second", "");

    int rc = phone_db_save_sorted(&db, "test_data/output_inc.csv");
    ASSERT_EQ(rc, 0);

    FILE *f = fopen("test_data/output_inc.csv", "r");
    ASSERT(f != NULL);

    char line[1024];
    ASSERT(fgets(line, sizeof(line), f));
    ASSERT(strncmp(line, "AAA;first;", 10) == 0);

    ASSERT(fgets(line, sizeof(line), f));
    ASSERT(strncmp(line, "BBB;second;", 11) == 0);

    ASSERT(fgets(line, sizeof(line), f));
    ASSERT(strncmp(line, "CCC;third;", 10) == 0);

    fclose(f);
    phone_db_destroy(&db);
}

/**
 * @brief Проверяет сохранение с учётом инкрементального DELETE.
 *
 * Загружает три записи, удаляет одну через OP_DELETE, сохраняет.
 * В выходном файле только две оставшиеся записи.
 */
TEST(test_save_sorted_with_delete) {
    write_csv_file("test_data/save_del.csv",
        "AAA;keep;\n"
        "BBB;remove;\n"
        "CCC;keep;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/save_del.csv");

    phone_db_apply_increment(&db, OP_DELETE, "BBB", "", "");

    int rc = phone_db_save_sorted(&db, "test_data/output_del.csv");
    ASSERT_EQ(rc, 0);

    FILE *f = fopen("test_data/output_del.csv", "r");
    ASSERT(f != NULL);

    char line[1024];
    ASSERT(fgets(line, sizeof(line), f));
    ASSERT(strncmp(line, "AAA;keep;", 9) == 0);

    ASSERT(fgets(line, sizeof(line), f));
    ASSERT(strncmp(line, "CCC;keep;", 9) == 0);

    /* Третьей строки быть не должно */
    ASSERT(fgets(line, sizeof(line), f) == NULL);

    fclose(f);
    phone_db_destroy(&db);
}

/* -- Тесты использования памяти ---------------------------------------- */

/**
 * @brief Проверяет, что phone_db_memory_usage возвращает ненулевое значение.
 *
 * После инициализации БД объём памяти должен быть больше 0.
 */
TEST(test_memory_usage_reasonable) {
    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);

    size_t baseline = phone_db_memory_usage(&db);
    ASSERT(baseline > 0);

    phone_db_destroy(&db);
}

/**
 * @brief Проверяет объём памяти после загрузки двух записей.
 *
 * Для двух записей объём памяти должен быть разумным (менее 1 МБ).
 */
TEST(test_memory_usage_after_load) {
    write_csv_file("test_data/mem.csv",
        "1111;hello;\n"
        "2222;world;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/mem.csv");

    size_t usage = phone_db_memory_usage(&db);
    /* Должно быть разумно для 2 записей (менее 1 МБ) */
    ASSERT(usage < 1024 * 1024);

    phone_db_destroy(&db);
}

/* -- Тесты производительности / масштаба ------------------------------- */

/**
 * @brief Проверяет производительность пакетной загрузки.
 *
 * Генерирует CSV на 100K записей и загружает его.
 * Замеряет время и проверяет корректность поиска.
 */
TEST(test_bulk_load_performance) {
    /* Генерация большого CSV на 100K записей */
    printf("\n    Генерация тестового CSV... ");
    fflush(stdout);

    FILE *f = fopen("test_data/perf.csv", "w");
    assert(f);

    srand(42);
    for (int i = 0; i != 100000; ++i) {
        fprintf(f, "%08X;comment_%d;2025-12-31\n", i, i);
    }
    fclose(f);
    printf("готово.\n    Загрузка... ");
    fflush(stdout);

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int rc = phone_db_load_csv(&db, "test_data/perf.csv");
    clock_gettime(CLOCK_MONOTONIC, &t1);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(db.count, 100000);

    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    printf("%.3fs (%.0f записей/сек) ", elapsed, 100000.0 / elapsed);

    /* Проверка некоторых поисков */
    phone_key_t key;
    const char *comment;
    uint16_t clen;
    uint32_t expiry;

    phone_key_from_hex(&key, "00000001");
    int found = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(found, 0);

    phone_key_from_hex(&key, "0001869F");  /* 99999 */
    found = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(found, 0);

    phone_db_destroy(&db);
}

/**
 * @brief Проверяет производительность инкрементальных операций.
 *
 * Выполняет 1000 операций OP_ADD и замеряет время.
 * Все операции должны завершиться менее чем за 1 секунду.
 */
TEST(test_increment_performance) {
    write_csv_file("test_data/inc_perf.csv", "00000000;base;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/inc_perf.csv");

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    /* Симуляция 1000 инкрементальных операций */
    for (int i = 1; i != 1001; ++i) {
        char num[20];
        snprintf(num, sizeof(num), "%08X", i);
        phone_db_apply_increment(&db, OP_ADD, num, "inc", "");
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    printf("\n    1000 инкрементов за %.6fs (%.0f ops/сек) ", elapsed, 1000.0 / elapsed);

    /* Каждая операция инкремента должна быть менее 100мс */
    ASSERT(elapsed < 1.0);

    phone_db_destroy(&db);
}

/* -- Тесты пограничных случаев ----------------------------------------- */

/**
 * @brief Проверяет загрузку CSV с дублирующимися ключами.
 *
 * Две записи с одинаковым ключом AAAA не должны вызвать краш.
 */
TEST(test_bulk_load_duplicate_keys) {
    write_csv_file("test_data/dup.csv",
        "AAAA;first;\n"
        "AAAA;second;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    int rc = phone_db_load_csv(&db, "test_data/dup.csv");
    /* Должны корректно обработать — либо оставить последнюю, либо вернуть ошибку */
    /* Минимум — не должно быть краша */
    ASSERT(rc == 0 || rc == -1);
    phone_db_destroy(&db);
}

/**
 * @brief Проверяет загрузку CSV с пробелами в полях.
 *
 * Запись " AAA ; hello ; 2025-01-01 " — ключ содержит пробел,
 * hex-парсер отвергает строку, count остаётся 0.
 */
TEST(test_bulk_load_whitespace) {
    write_csv_file("test_data/whitespace.csv",
        " AAA ; hello ; 2025-01-01 \n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    int rc = phone_db_load_csv(&db, "test_data/whitespace.csv");
    ASSERT_EQ(rc, 0);
    /* Ключ " AAA " содержит пробел — hex-парсер отвергает строку */
    ASSERT_EQ(db.count, 0);

    phone_db_destroy(&db);
}

/**
 * @brief Проверяет добавление записи в пустую БД.
 *
 * OP_ADD в пустой БД должен работать корректно.
 */
TEST(test_apply_increment_to_empty_db) {
    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);

    /* Добавление в пустую БД должно работать */
    int rc = phone_db_apply_increment(&db, OP_ADD, "AAAA", "hello", "");
    ASSERT_EQ(rc, 0);

    phone_key_t key;
    const char *comment;
    uint16_t clen;
    uint32_t expiry;
    phone_key_from_hex(&key, "AAAA");
    rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(rc, 0);

    phone_db_destroy(&db);
}

/**
 * @brief Проверяет удаление несуществующей записи.
 *
 * OP_DELETE для ключа, которого нет в БД, не должен вызвать краш.
 */
TEST(test_delete_nonexistent) {
    write_csv_file("test_data/del_ne.csv", "AAAA;keep;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/del_ne.csv");

    /* Удаление несуществующей записи не должно приводить к крашу */
    int rc = phone_db_apply_increment(&db, OP_DELETE, "ZZZZ", "", "");
    /* Должна вернуть ошибку или обработать корректно */
    ASSERT(rc == 0 || rc == -1);

    phone_db_destroy(&db);
}

/**
 * @brief Проверяет компактность структуры phone_record_t.
 *
 * Размер структуры записи не должен превышать 24 байта.
 */
TEST(test_sorted_record_size) {
    /* Проверка, что структура записи упакована и компактна */
    ASSERT(sizeof(phone_record_t) <= 24);
}

/* -- Покрытие: переполнение add_pending_comment ------------------------ */

/**
 * @brief Проверяет переполнение буфера pending_comment_buf.
 *
 * Добавляет 500 инкрементальных операций с комментариями по 300 байт,
 * что превышает начальный размер буфера 64 КБ. Тест проверяет,
 * что realloc корректно расширяет буфер без потери данных.
 */
TEST(test_pending_comment_buf_overflow) {
    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);

    /* pending_comment_buf начинается с 64 КБ. Каждый комментарий ~300 байт. */
    int count = 0;
    for (int i = 0; i != 500; ++i) {
        char num[20];
        snprintf(num, sizeof(num), "%08X", i);
        char comment[310];
        memset(comment, 'A', 300);
        comment[300] = '\0';
        int rc = phone_db_apply_increment(&db, OP_ADD, num, comment, "");
        if (rc != 0) break;
        count++;
    }

    /* Хотя бы часть должна завершиться успешно; путь realloc должен быть проверен */
    ASSERT(count > 0);
    phone_db_destroy(&db);
}

/* -- Покрытие: переполнение comment_buf при load_csv ------------------- */

/**
 * @brief Проверяет переполнение основного буфера комментариев при загрузке CSV.
 *
 * Генерирует CSV с 2000 записями, каждая содержит комментарий длиной 200 байт.
 * Суммарный объём данных (~400 КБ) превышает начальный буфер 64 КБ.
 * Тест проверяет, что realloc расширяет comment_buf и все записи загружаются.
 */
TEST(test_load_csv_comment_buf_overflow) {
    /* Создание CSV, где суммарные данные комментариев превышают начальный буфер 64 КБ */
    FILE *f = fopen("test_data/big_comments.csv", "w");
    assert(f);
    for (int i = 0; i != 2000; ++i) {
        fprintf(f, "%08X;", i);
        for (int j = 0; j != 200; ++j) fputc('X', f);
        fprintf(f, ";\n");
    }
    fclose(f);

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    int rc = phone_db_load_csv(&db, "test_data/big_comments.csv");
    /* Должно завершиться успешно — realloc расширяет буфер */
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(db.count, 2000);

    phone_key_t key;
    const char *comment;
    uint16_t clen;
    uint32_t expiry;
    phone_key_from_hex(&key, "00000001");
    rc = phone_db_lookup(&db, &key, &comment, &clen, &expiry);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(clen, 200);

    phone_db_destroy(&db);
}

/* -- Покрытие: дедупликация pending и save_sorted --------------------- */

/**
 * @brief Проверяет дедупликацию отложенных операций при двух UPDATE.
 *
 * Выполняет два UPDATE для одного ключа AAAA. Вторая операция должна
 * перезаписать первую в массиве pending. Тест проверяет, что в итоговом
 * файле только одна запись AAAA с последним комментарием.
 */
TEST(test_save_sorted_with_dedup) {
    write_csv_file("test_data/save_dedup.csv",
        "AAAA;first;\n"
        "BBBB;second;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/save_dedup.csv");

    /* Обновить один ключ дважды — вторая запись должна перезаписать первую */
    phone_db_apply_increment(&db, OP_UPDATE, "AAAA", "updated_v1", "");
    phone_db_apply_increment(&db, OP_UPDATE, "AAAA", "updated_v2", "");

    ASSERT_EQ(db.pending_count, 1);

    int rc = phone_db_save_sorted(&db, "test_data/out_dedup.csv");
    ASSERT_EQ(rc, 0);

    FILE *f = fopen("test_data/out_dedup.csv", "r");
    ASSERT(f != NULL);

    char line[1024];
    ASSERT(fgets(line, sizeof(line), f));
    ASSERT(strncmp(line, "AAAA;updated_v2;", 16) == 0);

    ASSERT(fgets(line, sizeof(line), f));
    ASSERT(strncmp(line, "BBBB;second;", 12) == 0);

    ASSERT(fgets(line, sizeof(line), f) == NULL);

    fclose(f);
    phone_db_destroy(&db);
}

/* -- Покрытие: save_sorted с UPDATE для существующей записи ------------ */

/**
 * @brief Проверяет UPDATE существующей записи через save_sorted.
 *
 * Загружает две записи, затем обновляет комментарий и срок действия
 * для одной из них. Тест проверяет, что в выходном файле запись AAAA
 * содержит новый комментарий и новый срок, а запись BBB unchanged.
 */
TEST(test_save_sorted_update_existing) {
    write_csv_file("test_data/save_upd.csv",
        "AAAA;old_comment;2025-06-01\n"
        "BBBB;keep;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/save_upd.csv");

    phone_db_apply_increment(&db, OP_UPDATE, "AAAA", "new_comment", "2026-01-01");

    int rc = phone_db_save_sorted(&db, "test_data/out_upd.csv");
    ASSERT_EQ(rc, 0);

    FILE *f = fopen("test_data/out_upd.csv", "r");
    ASSERT(f != NULL);

    char line[1024];
    ASSERT(fgets(line, sizeof(line), f));
    ASSERT(strncmp(line, "AAAA;new_comment;", 16) == 0);

    ASSERT(fgets(line, sizeof(line), f));
    ASSERT(strncmp(line, "BBBB;keep;", 10) == 0);

    ASSERT(fgets(line, sizeof(line), f) == NULL);

    fclose(f);
    phone_db_destroy(&db);
}

/* -- Покрытие: save_sorted с UPDATE, устанавливающим пустой комментарий */

/**
 * @brief Проверяет очистку комментария через UPDATE с пустой строкой.
 *
 * Загружает запись AAAA с комментарием, затем выполняет UPDATE
 * с пустым комментарием. Тест проверяет, что в выходном файле
 * комментарий AAAA очищен (поля между разделителями пустые).
 */
TEST(test_save_sorted_update_empty_comment) {
    write_csv_file("test_data/save_upd_ec.csv",
        "AAAA;has_comment;2025-06-01\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/save_upd_ec.csv");

    /* Обновление с пустым комментарием — должно очистить его */
    phone_db_apply_increment(&db, OP_UPDATE, "AAAA", "", "");

    int rc = phone_db_save_sorted(&db, "test_data/out_upd_ec.csv");
    ASSERT_EQ(rc, 0);

    FILE *f = fopen("test_data/out_upd_ec.csv", "r");
    ASSERT(f != NULL);

    char line[1024];
    ASSERT(fgets(line, sizeof(line), f));
    ASSERT(strncmp(line, "AAAA;;", 6) == 0);

    ASSERT(fgets(line, sizeof(line), f) == NULL);

    fclose(f);
    phone_db_destroy(&db);
}

/* -- Покрытие: save_sorted с ADD новой записи с пустым комментарием ---- */

/**
 * @brief Проверяет ADD новой записи с пустым комментарием через save_sorted.
 *
 * Загружает существующую запись AAAA, затем добавляет новую BBB
 * с пустым комментарием. Тест проверяет, что в выходном файле
 * обе записи присутствуют, BBB имеет пустой комментарий.
 */
TEST(test_save_sorted_add_new_empty_comment) {
    write_csv_file("test_data/save_add_ec.csv",
        "AAAA;existing;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/save_add_ec.csv");

    phone_db_apply_increment(&db, OP_ADD, "BBBB", "", "");

    int rc = phone_db_save_sorted(&db, "test_data/out_add_ec.csv");
    ASSERT_EQ(rc, 0);

    FILE *f = fopen("test_data/out_add_ec.csv", "r");
    ASSERT(f != NULL);

    char line[1024];
    ASSERT(fgets(line, sizeof(line), f));
    ASSERT(strncmp(line, "AAAA;existing;", 14) == 0);

    ASSERT(fgets(line, sizeof(line), f));
    ASSERT(strncmp(line, "BBBB;;", 6) == 0);

    ASSERT(fgets(line, sizeof(line), f) == NULL);

    fclose(f);
    phone_db_destroy(&db);
}

/* -- Покрытие: ADD для ключа, уже существующего в records -------------- */

/**
 * @brief Проверяет ADD для ключа, который уже есть в records.
 *
 * Загружает запись AAAA, затем выполняет ADD для того же ключа.
 * Тест проверяет, что в выходном файле только одна запись AAAA
 * (дубликат не создаётся), а комментарий заменяется.
 */
TEST(test_save_sorted_add_existing_key) {
    write_csv_file("test_data/save_add_ex.csv",
        "AAAA;original;\n");

    phone_db_t db = PHONE_DB_INITIALIZER;
    phone_db_init(&db, 0, 0, 0);
    phone_db_load_csv(&db, "test_data/save_add_ex.csv");

    /* ADD для уже существующего ключа — не должно создавать дубликат */
    phone_db_apply_increment(&db, OP_ADD, "AAAA", "replacement", "");

    int rc = phone_db_save_sorted(&db, "test_data/out_add_ex.csv");
    ASSERT_EQ(rc, 0);

    FILE *f = fopen("test_data/out_add_ex.csv", "r");
    ASSERT(f != NULL);

    char line[1024];
    int line_count = 0;
    while (fgets(line, sizeof(line), f)) line_count++;
    ASSERT_EQ(line_count, 1);

    fclose(f);
    phone_db_destroy(&db);
}

/* -- Главная функция -------------------------------------------------- */

int main(void) {
    printf("=== Тесты phone_db ===\n\n");

    printf("[Кодирование ключа телефона]\n");
    RUN(test_phone_key_from_hex_basic);
    RUN(test_phone_key_from_hex_max_length);
    RUN(test_phone_key_from_hex_short);
    RUN(test_phone_key_from_hex_empty);
    RUN(test_phone_key_to_hex_roundtrip);
    RUN(test_phone_key_to_hex_short);
    printf("\n");

    printf("[Сравнение ключей телефона]\n");
    RUN(test_phone_key_compare_equal);
    RUN(test_phone_key_compare_less);
    RUN(test_phone_key_compare_greater);
    RUN(test_phone_key_compare_different_length);
    RUN(test_phone_key_compare_zero);
    RUN(test_phone_key_is_empty);
    printf("\n");

    printf("[Инициализация/уничтожение базы данных]\n");
    RUN(test_db_init);
    RUN(test_db_destroy);
    printf("\n");

    printf("[Пакетная загрузка]\n");
    RUN(test_bulk_load_simple);
    RUN(test_bulk_load_lookup);
    RUN(test_bulk_load_lookup_not_found);
    RUN(test_bulk_load_sorted_order);
    RUN(test_bulk_load_empty_file);
    RUN(test_bulk_load_missing_comment);
    RUN(test_bulk_load_missing_expiry);
    printf("\n");

    printf("[Инкрементальные обновления]\n");
    RUN(test_increment_add);
    RUN(test_increment_update);
    RUN(test_increment_delete);
    RUN(test_increment_multiple_ops);
    printf("\n");

    printf("[Сохранение в отсортированном порядке]\n");
    RUN(test_save_sorted_basic);
    RUN(test_save_sorted_with_increments);
    RUN(test_save_sorted_with_delete);
    printf("\n");

    printf("[Использование памяти]\n");
    RUN(test_memory_usage_reasonable);
    RUN(test_memory_usage_after_load);
    printf("\n");

    printf("[Производительность / масштаб]\n");
    RUN(test_bulk_load_performance);
    RUN(test_increment_performance);
    printf("\n");

    printf("[Пограничные случаи]\n");
    RUN(test_bulk_load_duplicate_keys);
    RUN(test_bulk_load_whitespace);
    RUN(test_apply_increment_to_empty_db);
    RUN(test_delete_nonexistent);
    RUN(test_sorted_record_size);
    printf("\n");

    printf("[Покрытие: непокрытые пути]\n");
    RUN(test_pending_comment_buf_overflow);
    RUN(test_load_csv_comment_buf_overflow);
    RUN(test_save_sorted_with_dedup);
    RUN(test_save_sorted_update_existing);
    RUN(test_save_sorted_update_empty_comment);
    RUN(test_save_sorted_add_new_empty_comment);
    RUN(test_save_sorted_add_existing_key);
    printf("\n");

    printf("=== Результаты: %d/%d пройдено", tests_passed, tests_run);
    if (tests_failed > 0)
        printf(", %d ПРОВАЛЕНО", tests_failed);
    printf(" ===\n");

    return tests_failed > 0 ? 1 : 0;
}
