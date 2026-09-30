//This code is written to create a mini polling instrument from a Wiznet 5500 EVB Pico2. This was originally using an Arduino UNO Rev3 with ethernet shield, hence the naming.

//The Pico2 can then be polled at regular intervals from DAQFactory through TCP/IP.

//Add this to your 'init' sequence

global ARDENV_1 = new(arduino_env)
ARDENV_1.initTCP("XXX.XXX.X.XXX", YYYY, 1000) //replace "XXX.XXX.X.XXX" with your IP, replace YYYY with your port
ARDENV_1.setInstrumentName("ARDENV_1")
ARDENV_1.setSerialNumber("AE001")
ARDENV_1.setMessageStart("A") //the Pico expects an A over ASCII. when it receives this, it will return temperature and relative humidity, separated by a comma
ARDENV_1.createChannels()


//Add this into getParamNames sequence:

   case(instrumentType == "arduino_env")
      return({"temperature", "relative_humidity"})
            

//Add this in instrumentClass sequence:

class arduino_env parent pollingInstrument
   function onCreate()
      setInstrumentType("arduino_env")
      setMessageStart("A")
   endfunction

   function getAll()
      private string response
      private responseParsed

      try
         response = Poll(messageStart+chr(13), 13)
      catch()
         response = NULL
      endcatch

      if(!IsEmpty(response))
         response = parse(response, -1, ",")
         if(numRows(response) == numRows(paramNames))
            for(private i = 0, i < numRows(channelNames), i++)
               try
                  responseParsed = strToDouble(response[i])
                  execute(channelNames[i]+".addValue(responseParsed)")
               catch()
                  execute(channelNames[i]+".addValue(1/0)")
               endcatch
            endfor
         endif
      endif
   endfunction
endclass

