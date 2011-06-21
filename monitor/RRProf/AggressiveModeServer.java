package RRProf;

import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;

public class AggressiveModeServer extends Thread {

	MonitorGUI gui;
	ServerSocket serverSocket;
	boolean error;
	
	AggressiveModeServer(MonitorGUI gui) {
		this.gui = gui;
		error = false;
		setDaemon(true);
	}
	
	
	public void run() {
		try{
			serverSocket = new ServerSocket(12300);
		}
		catch(IOException e) {
			e.printStackTrace();
			error = true;
			return;
		}

		while(true) {
			try{
				Socket client = serverSocket.accept();
				gui.newAgressiveConnected(client);
			} catch(IOException e){
				e.printStackTrace();
				error = true;
				return;
			}
		}
	}
}
