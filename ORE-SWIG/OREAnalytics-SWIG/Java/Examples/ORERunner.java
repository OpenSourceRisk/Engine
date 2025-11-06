/*
  Copyright (C) 2018 Quaternion Risk Management 
  All rights reserved.
*/

/*! Demo Java application */

package orerunner;

import oreanalytics.*;
import java.io.*;
import java.nio.*;
import java.util.*;

public class ORERunner {
   
    /*! Load wrapper library */
    
    static {
	System.loadLibrary("OREAnalyticsJava");
    }

    /*! main method */

    public static void main(String argv[]) {
	try {
	    System.out.println("============================================");
	    System.out.println("ORE Java started...");
	    System.out.println("============================================");

	    if (argv.length < 1) {
		System.out.println("Usage: ORERunner path/to/ore.xml");
		System.exit(0);
	    }

	    System.out.println("Load Application Parameters");    
	    Parameters p = new Parameters();
	    p.fromFile(argv[0]);

	    System.out.println("Initialize ORE App");    
	    OREApp app = new OREApp(p);
	    
	    System.out.println("Done.");    
	}
	catch(Exception e){
	    System.out.println("Exception caught: " + e.getMessage());
	    e.printStackTrace();
	}
    }

};
