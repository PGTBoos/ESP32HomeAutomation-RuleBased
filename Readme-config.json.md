For this all to work you should make reservations in you own DHCP server.   
(your internet router).  

As to assign static ip addresses to certain hardware.  
This code depends on a file `config.json` that must be placed in the data folder.  

It should contain ip addresses for your [home wizard devices](https://www.homewizard.com/nl/shop/) :

{  
    "wifi_ssid": "YOUR WIFI NAME OF YOUR HOME",  
    "wifi_password": "YOUR PASSWORD OF YOUR WIFI NETWORK",  
    "p1_ip": "192.168.178.40",  
    "socket_1": "192.168.178.50",  
    "socket_2": "192.168.178.60",  
    "socket_3": "192.168.178.70",  
    "socket_4": "192.168.178.80",  
    "phone_ip": "192.168.178.199",  
    "PC_ip": "192.168.178.200",  
    "power_on_threshold": 1000,  
    "power_off_threshold": 990,  
    "min_on_time": 300,  
    "min_off_time": 300,  
    "max_on_time": 1800  
}  
  
