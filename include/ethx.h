#ifndef _ETHX_H_
#define _ETHX_H_


#define ETHX_NAME "ethx"
#define MAX_SLAVE 1

#define ethx_handler_get_rcu(dev) \
	((struct net_device *) rcu_dereference(dev->rx_handler_data))

static const char *SLAVE_DEV[MAX_SLAVE] = 
{
	"eth0"
	//"eth1"
};

struct slave
{
	u8     hw_addr[ETH_ALEN];
	struct net_device *dev;
};

struct master
{
	struct net_device *dev;
	struct slave slave[MAX_SLAVE];
	int slave_current_active;
	int slave_count;
	spinlock_t lock;
};


#endif