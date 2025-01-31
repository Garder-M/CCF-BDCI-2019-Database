#include "../common.h"

int main(int argc, char* argv[])
{
    CHECK(argc == 2, "debug_preload <txt_file>");

    const char* const file_path = argv[1];

    int shmkey;
    if (strstr(file_path, "customer")) {
        shmkey = SHMKEY_TXT_CUSTOMER;
    }
    else if (strstr(file_path, "orders")) {
        shmkey = SHMKEY_TXT_ORDERS;
    }
    else if (strstr(file_path, "lineitem")) {
        shmkey = SHMKEY_TXT_LINEITEM;
    }
    else {
        PANIC("unknown file %s: customer? orders? lineitem?", file_path);
    }
    INFO("shmkey: %d", shmkey);

    load_file_context ctx;
    __open_file_read_direct(file_path, &ctx);
    INFO("file opened: %s", file_path);
    INFO("file size: %lu (0x%lx)", ctx.file_size, ctx.file_size);

    // NOTE: We do NOT use huge page here!
    const int shmid = C_CALL(shmget(
        shmkey,
        ctx.file_size,
        IPC_CREAT | 0666 | SHM_NORESERVE | SHM_HUGETLB | SHM_HUGE_2MB));
    INFO("shmid: %d", shmid);

    void* const ptr = (uint8_t*)shmat(shmid, nullptr, 0);
    INFO("shmat got %p", ptr);
    CHECK(ptr != (void*)-1,
        "shmat(shmid=0x%x) failed. errno = %d (%s)", shmid, errno, strerror(errno));

    constexpr const uint64_t STEP = 1024 * 1024 * 1024;
    uint64_t offset = 0;
    while (offset < ctx.file_size) {
        const size_t size = C_CALL(pread(
            ctx.fd,
            (void*)((uintptr_t)ptr + offset),
            STEP,
            offset));
        CHECK(size <= STEP);
        CHECK(size <= ctx.file_size - offset);
        //__fadvice_dont_need(ctx.fd, offset, size);
        offset += size;
        INFO("pread() %lu bytes, totally %lu bytes", size, offset);
    }
    INFO("pread() all %lu bytes done", ctx.file_size);

    C_CALL(shmdt(ptr));
    INFO("shmdt() done");

    //__fadvice_dont_need(ctx.fd, 0, ctx.file_size);
    //INFO("__fadvice_dont_need() done");

    return 0;
}
