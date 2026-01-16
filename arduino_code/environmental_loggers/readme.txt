//This code is written to create a mini polling instrument from an arduino using an ethernet shield V2. 

//The arduino can then be polled at regular intervals from DAQFactory, and communicate by TCP/IP.

//Add this in instrumentClass sequence:

class arduino_env parent pollingInstrument
   function onCreate()
      setInstrumentType("arduino_env")
      setMessageStart("") 
   endfunction

   function parseResponse(string response)
      return(strToDouble(response))
   endfunction
endclass

