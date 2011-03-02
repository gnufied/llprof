package RRProf;
import java.io.*;
import java.net.*;


class MonitorTest {
	private static Monitor mon;
	
	public static void main(String[] args) throws Exception {
		String server = args[0];
		int sleep_time = Integer.parseInt(args[1]);
		int port = 12321;


		mon = new Monitor();
        mon.startProfile(server, port);

       while(true)
        {
            Thread.sleep(sleep_time);
	        mon.getProfileData();
	        mon.printProfileData();
	        mon.checkData();
        }
		
	}
}

