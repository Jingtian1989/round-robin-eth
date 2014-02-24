#include <linux/module.h>
#include <linux/init.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/list.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/if_arp.h>

#include "ethx.h"

static struct master *master;

static int vdev_master_netdev_event(unsigned long event, struct net_device *master_dev)
{
	return NOTIFY_DONE;
}
static int vdev_slave_netdev_event(unsigned long event, struct net_device *slave_dev)
{
	switch (event) {
	case NETDEV_UNREGISTER:
		break;
	case NETDEV_UP:
	case NETDEV_CHANGE:
		break;
	case NETDEV_DOWN:
		break;
	case NETDEV_CHANGEMTU:
		break;
	case NETDEV_CHANGENAME:
		break;
	case NETDEV_FEAT_CHANGE:
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static int master_netdev_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct net_device *event_dev = (struct net_device *)ptr;

	if (event_dev->flags & IFF_MASTER) {
		printk(KERN_ALERT "[ETHX] MASTER device.\n");
		return vdev_master_netdev_event(event, event_dev);
	}

	if (event_dev->flags & IFF_SLAVE) {
		pr_debug("[ETHX] SLAVE device.\n");
		return vdev_slave_netdev_event(event, event_dev);
	}

	return NOTIFY_DONE;
}


static void master_rand_next(struct master *master)
{
	master->slave_current_active = (master->slave_current_active + 1) % master->slave_count;
}


static int master_open(struct net_device *master_dev)
{
	netif_carrier_on(master->dev);
	netif_start_queue(master->dev);
	return 0;
}

static int master_close(struct net_device *master_dev)
{
	netif_carrier_off(master->dev);
	netif_stop_queue(master->dev);
	return 0;
}


static int master_do_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	return 0;
}

static netdev_tx_t master_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct master *master = netdev_priv(dev);
	skb->dev = master->slave[master->slave_current_active].dev;
	master_rand_next(master);
	//printk(KERN_ALERT "[ETHX] master_start_xmit.\n");
	return dev_queue_xmit(skb);
}

static struct rtnl_link_stats64 *master_get_stats(struct net_device *dev, struct rtnl_link_stats64 *stats)
{
	struct rtnl_link_stats64 temp;
	struct master *master = netdev_priv(dev);
	struct rtnl_link_stats64 *sstats;
	int i = 0;
	memset(stats, 0, sizeof(*stats));

	for(i = 0 ; i < master->slave_count ; i++)
	{
		sstats = dev_get_stats(master->slave[i].dev, &temp);
		stats->rx_packets += sstats->rx_packets;
		stats->rx_bytes += sstats->rx_bytes;
		stats->rx_errors += sstats->rx_errors;
		stats->rx_dropped += sstats->rx_dropped;

		stats->tx_packets += sstats->tx_packets;
		stats->tx_bytes += sstats->tx_bytes;
		stats->tx_errors += sstats->tx_errors;
		stats->tx_dropped += sstats->tx_dropped;

		stats->multicast += sstats->multicast;
		stats->collisions += sstats->collisions;

		stats->rx_length_errors += sstats->rx_length_errors;
		stats->rx_over_errors += sstats->rx_over_errors;
		stats->rx_crc_errors += sstats->rx_crc_errors;
		stats->rx_frame_errors += sstats->rx_frame_errors;
		stats->rx_fifo_errors += sstats->rx_fifo_errors;
		stats->rx_missed_errors += sstats->rx_missed_errors;

		stats->tx_aborted_errors += sstats->tx_aborted_errors;
		stats->tx_carrier_errors += sstats->tx_carrier_errors;
		stats->tx_fifo_errors += sstats->tx_fifo_errors;
		stats->tx_heartbeat_errors += sstats->tx_heartbeat_errors;
		stats->tx_window_errors += sstats->tx_window_errors;
	}

	return stats;
}


static int master_set_mac_address(struct net_device *master_dev, void *addr)
{
	struct sockaddr *sa = addr;
	memcpy(master_dev->dev_addr, sa->sa_data, master_dev->addr_len);
	return 0;
}

static const struct net_device_ops master_netdev_ops = 
{
	.ndo_open		= master_open,
	.ndo_stop		= master_close,
	.ndo_do_ioctl		= master_do_ioctl,
	.ndo_start_xmit		= master_start_xmit,
	.ndo_get_stats64	= master_get_stats,
	.ndo_set_mac_address	= master_set_mac_address,

};

static void master_setup(struct net_device *master_dev)
{
	struct master *master = netdev_priv(master_dev);
	master->dev = master_dev;

	spin_lock_init(&master->lock);
	master->slave_current_active = -1;
	master->slave_count = 0;


	ether_setup(master_dev);
	master_dev->netdev_ops = &master_netdev_ops;

	master_dev->tx_queue_len = 0;
	master_dev->flags |= IFF_MASTER | IFF_MULTICAST;

	//block add vlan
	master_dev->features |= NETIF_F_VLAN_CHALLENGED;

	//only add hw csum
	master_dev->hw_features &= ~(NETIF_F_ALL_CSUM & ~NETIF_F_HW_CSUM);
	master_dev->features |= master_dev->hw_features;
}




static int master_set_slave_addr(struct master *master, struct slave *slave, int first)
{
	struct sockaddr saddr;
	struct net_device *master_dev;
	struct net_device *slave_dev = slave->dev;
	saddr.sa_family = slave_dev->type;
	if(!master)
	{
		memcpy(saddr.sa_data, slave->hw_addr, slave_dev->addr_len);
		return dev_set_mac_address(slave_dev, &saddr);
	}
	master_dev = master->dev;
	memcpy(slave->hw_addr, slave_dev->dev_addr, slave_dev->addr_len);
	if(first)
	{
		memcpy(saddr.sa_data, slave_dev->dev_addr, slave_dev->addr_len);
		return dev_set_mac_address(master_dev, &saddr);
	}
	memcpy(saddr.sa_data, master_dev->dev_addr, master_dev->addr_len);	
	return dev_set_mac_address(slave_dev, &saddr);
}


static void master_release_slave(struct master *master, int count)
{
	int i = 0;
	for(i = 0 ; i < count ; i++)
	{
		rtnl_lock();
		master->slave[i].dev->flags &= ~IFF_SLAVE;
		master_set_slave_addr(NULL, &master->slave[i], 0);
		netdev_rx_handler_unregister(master->slave[i].dev);
		rtnl_unlock();
		dev_put(master->slave[i].dev);
		master->slave[i].dev = NULL;
		master->slave_count--;
	}
}

static rx_handler_result_t ethx_handle_frame(struct sk_buff **pskb)
{
	struct sk_buff *skb = *pskb;
	struct net_device *dev = ethx_handler_get_rcu(skb->dev);
	skb->dev = dev;
	//printk(KERN_ALERT "[ETHX] ethx_handle_frame.\n");
	return RX_HANDLER_ANOTHER;
}


static int master_enslaves(struct net_device *master_dev)
{
	struct master *master = netdev_priv(master_dev);
	struct net_device *slave_dev;
	int i = 0, ret = 0;
	for(i = 0 ; i < MAX_SLAVE; i++)
	{
		slave_dev = dev_get_by_name(&init_net, SLAVE_DEV[i]);
		if(!slave_dev)
		{
			ret = -ENODEV;
			printk(KERN_ALERT "[ETHX] dev_get_by_name failed.\n");
			goto err_get_dev;
		}
		master->slave[i].dev = slave_dev;

		if((ret = master_set_slave_addr(master, &master->slave[i], !i)))
		{
			rtnl_unlock();
			master->slave[i].dev = NULL;
			dev_put(slave_dev);
			printk(KERN_ALERT "[ETHX] master_set_slave_addr failed.\n");
			goto err_set_addr;
		}
		
		if((ret = netdev_rx_handler_register(slave_dev, ethx_handle_frame, master_dev)))
		{
			rtnl_unlock();
			master_set_slave_addr(NULL, &master->slave[i], 0);
			master->slave[i].dev = NULL;
			dev_put(slave_dev);
			printk(KERN_ALERT "[ETHX] netdev_rx_handler_register failed.\n");
			goto err_register_handler;
		}

		slave_dev->flags |= IFF_SLAVE;
		master->slave_count++;
	}
	return ret;

err_register_handler:
err_set_addr:
err_get_dev:
	master_release_slave(master, i);
	return ret;
}


static int master_create(const char *name)
{
	struct net_device *master_dev;
	int ret = 0;
	master_dev = alloc_netdev(sizeof(struct master), name, master_setup);
	
	if(!master_dev)
	{
		ret = -ENOMEM;
		printk(KERN_ALERT "[ETHX] master_create failed.\n");
		goto err_alloc_dev;
	}
	master = netdev_priv(master_dev);
	dev_net_set(master_dev, &init_net);

	rtnl_lock();
	if((ret = register_netdevice(master_dev)))
	{
		rtnl_unlock();
		printk(KERN_ALERT "[ETHX] register_netdevice failed.\n");
		goto err_register_dev;
	}
	netif_carrier_off(master_dev);

	if((ret = master_enslaves(master_dev)))
	{
		printk(KERN_ALERT "[ETHX] master_enslaves failed.\n");
		goto err_master_enslaves;
	}
	master->slave_current_active = 0;
	rtnl_unlock();

	return ret;
err_master_enslaves:
	unregister_netdev(master_dev);
err_register_dev:
	free_netdev(master_dev);
err_alloc_dev:
	return ret;

}

static struct notifier_block master_netdev_notifier = {
	.notifier_call = master_netdev_event,
};

static int ethx_init_module(void)
{
	int ret = 0;
	if((ret = master_create(ETHX_NAME)))
	{
		printk(KERN_ALERT "[ETHX] master_create failed.\n");
		goto err_create_master;
	}
	register_netdevice_notifier(&master_netdev_notifier);
	return ret;
err_create_master:
	return ret;

}


static void ethx_exit_module(void)
{
	unregister_netdevice_notifier(&master_netdev_notifier);
	master_release_slave(master, master->slave_count);
	unregister_netdev(master->dev);
	free_netdev(master->dev);
}



module_init(ethx_init_module);
module_exit(ethx_exit_module);
MODULE_LICENSE("GPL");



