#ifndef ETHERNET_H
#define ETHERNET_H

#define INTERNAL_ETHERNETS_NUM      1

// Deklarace veřejných funkcí komponenty
void ethernet_connect(void);
// Můžeme implemenntovat i ethernet_stop, pokud bychom komunikovali po síti jen občas a chtěli šetřit prostředky

#endif