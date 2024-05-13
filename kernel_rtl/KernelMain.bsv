import FIFO::*;
import Vector::*;

typedef 2 MemPortCnt;

interface MemPortIfc;
	method ActionValue#(MemPortReq) readReq;
	method ActionValue#(MemPortReq) writeReq;
	method ActionValue#(Bit#(512)) writeWord;
	method Action readWord(Bit#(512) word);
endinterface

interface KernelMainIfc;
	method Action start(Bit#(32) param);
	method Bool done;
	interface Vector#(MemPortCnt, MemPortIfc) mem;
endinterface

typedef struct {
	Bit#(64) addr;
	Bit#(32) bytes;
} MemPortReq deriving (Eq,Bits);

typedef 128 Kval;
typedef 128 Wval;
//typedef 35 Kval;
//typedef 40 Wval;

module mkKernelMain(KernelMainIfc);
	Vector#(MemPortCnt, FIFO#(MemPortReq)) readReqQs <- replicateM(mkFIFO);
	Vector#(MemPortCnt, FIFO#(MemPortReq)) writeReqQs <- replicateM(mkFIFO);
	Vector#(MemPortCnt, FIFO#(Bit#(512))) writeWordQs <- replicateM(mkFIFO);
	Vector#(MemPortCnt, FIFO#(Bit#(512))) readWordQs <- replicateM(mkFIFO);
	Reg#(Bit#(32)) cycleCounter <- mkReg(0);
	Reg#(Bool) started <- mkReg(False);
	Reg#(Bit#(32)) bytesToRead <- mkReg(0);
	rule incCycle;
		cycleCounter <= cycleCounter + 1;
	endrule

	//////////////////////////////////////////////////////////////////////////
	Reg#(Bit#(32)) readReqOff <- mkReg(0);
	rule sendReadReq (bytesToRead > 0);
		if ( bytesToRead > 64 ) bytesToRead <= bytesToRead - 64;
		else bytesToRead <= 0;

		readReqQs[0].enq(MemPortReq{addr:zeroExtend(readReqOff), bytes:64});
		readReqOff <= readReqOff+ 64;
	endrule
	Reg#(Bit#(512)) serializerBuff <- mkReg(0);
	Reg#(Bit#(8)) serializerCouter <- mkReg(0);
	//FIFO#(Bit#(128)) serialInQ <- mkFIFO;
	FIFO#(Bit#(8)) compactedQ <- mkFIFO;
	rule recvReadResp;
		if ( serializerCouter == 0 ) begin
			let d = readWordQs[0].first;
			readWordQs[0].deq;
			compactedQ.enq(truncate(d));
			serializerBuff <= (d>>8);
			serializerCouter <= 63;
		end else begin
			serializerCouter <= serializerCouter - 1;
			serializerBuff <= (serializerBuff>>8);
			compactedQ.enq(truncate(serializerBuff));
		end
	endrule

	Vector#(Kval, Reg#(Bit#(8))) kwin <- replicateM(mkReg(?));
	Vector#(Kval, Reg#(Bit#(32))) kwin_partialhash <- replicateM(mkReg(?));
	FIFO#(Bit#(32)) hashQ <- mkFIFO;
	rule procKWin;
		compactedQ.deq;
		let d = compactedQ.first;

		for ( Integer i = 0; i < valueOf(Kval)-1; i = i + 1) begin
			kwin[i] <= kwin[i+1];
			Bit#(32) hash = kwin_partialhash[i+1]<<5 + kwin_partialhash[i+1] + zeroExtend(kwin[i+1]);
			kwin_partialhash[i] <= hash;
			if( i == 0 ) hashQ.enq(hash);
		end
		kwin[valueOf(Kval)-1] <= d;
		kwin_partialhash[valueOf(Kval)-1] <= 5381; // djb2 hash numbers
	endrule

	Vector#(Wval, Reg#(Bit#(32))) wwin_hashes <- replicateM(mkReg(?));
	Vector#(Wval, Reg#(Bit#(32))) wwin_offset <- replicateM(mkReg(?));

	Reg#(Bit#(8)) wwin_cycleCnt <- mkReg(0);
	FIFO#(Bit#(32)) winnowQ <- mkFIFO;
	rule shiftMin;
		if ( wwin_cycleCnt + 1 >= fromInteger(valueOf(Wval)) ) wwin_cycleCnt <= 0;
		else wwin_cycleCnt <= wwin_cycleCnt + 1;

		Bit#(32) hash = hashQ.first;
		hashQ.deq;

		for ( Integer i = 0; i < valueOf(Wval); i=i+1 ) begin
			if ( wwin_cycleCnt == fromInteger(i) ) begin
				wwin_hashes[i] <= hash;
				winnowQ.enq(wwin_hashes[i]);
			end else begin
				if ( wwin_hashes[i] > hash ) wwin_hashes[i] <= hash;
			end
		end
	endrule
	

   //int Kval = 35;
   //int Wval = 40;

	Reg#(Bit#(32)) emitcnt <- mkReg(0);
	Reg#(Bit#(512)) emitdes <- mkReg(0);
	rule emithash;
		if ( emitcnt >= 63 ) begin
			emitcnt <= 0;
			writeReqQs[1].enq(MemPortReq{addr:0, bytes:64});
			writeWordQs[1].enq(emitdes);
		end else emitcnt <= emitcnt + 1;

		winnowQ.deq;
		emitdes <= (emitdes<<32) | zeroExtend(winnowQ.first);
	endrule


	
	

	//////////////////////////////////////////////////////////////////////////




	Reg#(Bool) kernelDone <- mkReg(False);
	
	Vector#(MemPortCnt, MemPortIfc) mem_;
	for (Integer i = 0; i < valueOf(MemPortCnt); i=i+1) begin
		mem_[i] = interface MemPortIfc;
			method ActionValue#(MemPortReq) readReq;
				readReqQs[i].deq;
				return readReqQs[i].first;
			endmethod
			method ActionValue#(MemPortReq) writeReq;
				writeReqQs[i].deq;
				return writeReqQs[i].first;
			endmethod
			method ActionValue#(Bit#(512)) writeWord;
				writeWordQs[i].deq;
				return writeWordQs[i].first;
			endmethod
			method Action readWord(Bit#(512) word);
				readWordQs[i].enq(word);
			endmethod
		endinterface;
	end
	method Action start(Bit#(32) param);
		started <= True;
		bytesToRead <= param;
	endmethod
	method Bool done;
		return kernelDone;
	endmethod
	interface mem = mem_;
endmodule

