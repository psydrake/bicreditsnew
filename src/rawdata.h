#ifndef RAWDATA_H
#define RAWDATA_H

#include <iostream>
#include <math.h>
#include "util.h"
#include "init.h"
#include "wallet.h"
#include "main.h"
#include "coins.h"
#include "rpcserver.h"
#include "bidtracker.h"

using namespace std;

class Rawdata 
{
  public:
	
	int onehour, oneday, oneweek ,onemonth, oneyear;
  
	int totalnumtx();  //total number of chain transactions
	
	int incomingtx();
	
	int outgoingtx();
	
	int getNumTransactions() const ;//number of wallet transactions
	
	bool verifynumtx ();
	
	int netheight ();
	
	double networktxpart(); //wallet's network participation
	
	double lifetime(); //wallet's lifetime

	int gbllifetime(); //blockchain lifetime
	
	CAmount balance(); 
	
	CAmount getltcreserves();
	
	CAmount getbtcreserves();
	
	CAmount getbcrreserves();
	
	CAmount getdashreserves();
		
	CAmount Getgrantbalance();
	
	CAmount Getgrantstotal();

	CAmount Getgblavailablecredit(); 

	CAmount Getglobaldebt();

	CAmount Getgblmoneysupply();

};

#endif //RAWDATA_H
