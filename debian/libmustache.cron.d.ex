#
# Regular cron jobs for the libmustache package
#
0 4	* * *	root	[ -x /usr/bin/libmustache_maintenance ] && /usr/bin/libmustache_maintenance
