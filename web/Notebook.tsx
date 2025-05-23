import React, {ReactElement, useEffect, useRef, useState} from "react";
import {Alert, Container, Divider, Paper, Stack} from "@mui/material";

import {AddEntry, Entry, TextEntry, emptyAddEnt} from "./Entry";

import {DragStack} from "./DragStack";
import {DragHandle} from "@mui/icons-material";

type DisplayEntry = {ty: "ref", id: number, to: number} | {ty: "txt", id: number, txt: string};

type NotebookData = {
	entries: Entry[],
	dispEntries: DisplayEntry[],
	toAddEnt: AddEntry,
	nextEntryId: React.MutableRefObject<number>,
	nb: any
};

let addAddEnt = (CAS, nb, addEnt) => nb.addEntry(addEnt.name, addEnt.value, addEnt.len, CAS.OutputKind[addEnt.output_kind], addEnt.float_prec);

function makeNotebook(CAS: any, defaultEntries: AddEntry[], dispEntries: DisplayEntry[], nextEntryId): NotebookData {
	let nb = new CAS.Notebook();

	for (let d of dispEntries)
		if (d.ty=="ref" && d.id>=nextEntryId.current) nextEntryId.current=d.id+1;

	let entries: Entry[] = defaultEntries.map((addEnt) => {
		let ptr = addAddEnt(CAS, nb, addEnt);
		return {...addEnt, ptr, output_step: 0, loading: true, add: false};
	});

	return {entries, nb, dispEntries, toAddEnt: emptyAddEnt, nextEntryId};
}

function delNotebook(data: NotebookData) {
	for (let ent of data.entries) ent.ptr.delete();
	data.nb.delete();
}

interface NotebookProps {
	data: NotebookData, setData: (data: NotebookData) => void,
	CAS: any
}

function Notebook({CAS, data, setData}: NotebookProps) {
	let [toAddEnt, setToAddEnt] = useState<AddEntry>(emptyAddEnt);

	let entries =data.entries, nb=data.nb, dispEntries=data.dispEntries;

	let new_data = data;
	let setEntries = (entries) => setData(new_data = {...new_data, entries});
	let setDispEntries = (dispEntries) => setData(new_data = {...new_data, dispEntries});

	let update = (entry: Entry): [Entry, boolean] => {
		let stat = entry.ptr.status();
		let newent = {...entry, error: undefined};

		if (stat==CAS.Status.Loading && !newent.loading) {
			newent.loading=true;
		} else if (stat==CAS.Status.Updated) {
			newent.loading=false;

			let err_ptr = entry.ptr.getError();

			if (err_ptr.length>0) {
				newent.error = err_ptr;
			} else {
				let output: string = entry.ptr.getOutput().trim();
				newent.tex_input = entry.ptr.texInput();
				newent.output = output.split("\n");
				while (newent.output.length>0 && newent.output[newent.output.length-1]=="")
					newent.output.pop();
				newent.output_step++;
			}
		} else {
			return [entry, false];
		}

		return [newent, true];
	};

	let updateEntries = (newEntries, forceUpdate=false) => {
		newEntries = newEntries.map((x) => {
			let [y,z] = update(x);
			if (z) forceUpdate=true;
			return y;
		});

		if (forceUpdate) setEntries(newEntries);
	};

	useEffect(() => {
		let int = window.setInterval(() => {
			updateEntries(entries);
		}, 500);

		return () => window.clearTimeout(int);
	}, [entries]);

	let addEnt = (ent: AddEntry) => {
		console.log(ent);
		let ptr = addAddEnt(CAS, nb, ent);

		updateEntries([...entries, {...ent, ptr, output_step: 0, loading: true}], true);
		setDispEntries([...dispEntries, {ty: "ref", id: data.nextEntryId.current++, to: entries.length}]);
	};

	let setEnt = (idx, ent?: AddEntry) => {
		let arr_cpy = [...entries];
		let oldent = arr_cpy[idx];
		if (ent==null) {
			nb.removeEntry(oldent.ptr);
			oldent.ptr.delete();
			arr_cpy.splice(idx,1);

			let newDispEntries = [...dispEntries];
			for (let i=0; i<newDispEntries.length; i++) {
				let d = newDispEntries[i];
				if (d.ty=="ref") {
					if (d.to==idx) newDispEntries.splice(i,1), i--;
					else if (d.to>idx) d.to--;
				}
			}

			setDispEntries(newDispEntries);
		} else {
			let update_props: (keyof AddEntry)[] = ["value", "len", "output_kind", "float_prec"];

			if (ent.name!=oldent.name)
				nb.renameEntry(oldent.ptr, ent.name);

			for (let prop of update_props) {
				if (oldent[prop]==ent[prop]) continue;
				nb.updateEntry(oldent.ptr, ent.value, ent.len, CAS.OutputKind[ent.output_kind], ent.float_prec);
				break;
			}

			arr_cpy[idx]={...oldent, ...ent};
		}

		updateEntries(arr_cpy, true);
	};

	let [draggable, setDraggable] = useState(false);

	let ents = dispEntries.map((ent, idx) => {
		if (ent.ty=="txt") return {key: `${ent.id}`, el: <TextEntry txt={ent.txt} setDraggable={setDraggable} delEnt={() => {
				let arr_cpy = [...dispEntries];
				arr_cpy.splice(idx,1);
				setDispEntries(arr_cpy);
			}} />};
		else return {key: `${ent.id}`, el: <Entry setDraggable={setDraggable} entry={{...entries[ent.to], add:false}} setEnt={(newent) => setEnt(ent.to,newent)} ></Entry>};
	});

	let list: ReactElement[] = [];
	if (entries.length==0) {
		list.push(<Alert key={"alert"} severity="info" >Make an equation below -- it will appear here</Alert>);
	}

	list.push(<Entry setDraggable={setDraggable} key={"add"} entry={{...toAddEnt, add: true}} setEnt={(ent) => ent && setToAddEnt(ent)} addEnt={addEnt} ></Entry>);

	return (
		<Stack spacing={2} divider={<Divider orientation="horizontal" flexItem />} >
			<DragStack draggable={draggable} elements={ents} vertical={true} moveElement={(i,to) => {
				let arr_cpy = [...dispEntries];
				let [removed] = arr_cpy.splice(i,1);
				arr_cpy.splice(to,0,removed);

				setDispEntries(arr_cpy);
			}} ></DragStack>
			{list}
		</Stack>
	);
}

export {DisplayEntry, Notebook, NotebookProps, NotebookData, makeNotebook, delNotebook};