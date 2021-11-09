#include "vmlinux.h"
#include "bpf/bpf_helpers.h"
#include "bpf/bpf_core_read.h"
#include "bpf/bpf_tracing.h"

char __license[] SEC("license") = "Dual MIT/GPL";

struct dimensions_t {
	u32 saddr;
	u32 daddr;
} __attribute__((packed));

// const volatile uid_t filter_uid = -1;
// const volatile pid_t filter_pid = 0;

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 8192);
	__type(key, u32);
	__type(value, struct sock *);
	__uint(map_flags, BPF_F_NO_PREALLOC);
} sockets SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 8192);
	__type(key, struct dimensions_t);
	__type(value, u64);
} sockets_ext SEC(".maps.print");

static __always_inline int
enter_tcp_connect(struct pt_regs *ctx, struct sock *sk)
{
	__u64 pid_tgid = bpf_get_current_pid_tgid();
	__u32 tid = pid_tgid;
	bpf_printk("enter called");

	bpf_printk("enter: setting sk for tid: %u", tid);
	bpf_map_update_elem(&sockets, &tid, &sk, 0);
	return 0;
}

static __always_inline int
exit_tcp_connect(struct pt_regs *ctx, int ret)
{
	__u64 pid_tgid = bpf_get_current_pid_tgid();
	__u32 tid = pid_tgid;
	struct sock **skpp;
	struct sock *sk;

	__u32 saddr;
	__u32 daddr;
	struct dimensions_t key = {};
	u64 val = 1;

	bpf_printk("exit: getting sk for tid: '%u', ret is: '%d'", tid, ret);
	skpp = bpf_map_lookup_elem(&sockets, &tid);
	if (!skpp) {
		bpf_printk("exit: no pointer for tid, returning: %u", tid);
		return 0;
	}
	sk = *skpp;

	bpf_printk("exit: found sk for tid: %u", tid);
	BPF_CORE_READ_INTO(&saddr, sk, __sk_common.skc_rcv_saddr);
	BPF_CORE_READ_INTO(&daddr, sk, __sk_common.skc_daddr);

	bpf_printk("saddr: %u", saddr);
	bpf_printk("daddr: %u", daddr);

	key.saddr = saddr;
	key.daddr = daddr;
	// key.saddr = 1337;
	// key.daddr = 31337;
	bpf_map_update_elem(&sockets_ext, &key, &val, 0);
	// bpf_map_delete_elem(&sockets, &tid);
	return 0;
}

SEC("kprobe/tcp_v4_connect")
int BPF_KPROBE(tcp_v4_connect, struct sock *sk)
{
	return enter_tcp_connect(ctx, sk);
}

SEC("kretprobe/tcp_v4_connect")
int BPF_KRETPROBE(tcp_v4_connect_ret, int ret)
{
	return exit_tcp_connect(ctx, ret);
}
