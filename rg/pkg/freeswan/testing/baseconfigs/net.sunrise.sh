#
# $Id: net.sunrise.sh,v 1.2 2003/11/30 14:40:53 derry Exp $
#
if [ -n "$UML_private_CTL" ]
then
    net_eth0="eth0=daemon,10:00:00:dc:bc:01,unix,$UML_private_CTL,$UML_private_DATA";
else
    net_eth0="eth0=mcast,10:00:00:dc:bc:01,239.192.0.1,21200"
fi

net="$net_eth0"



