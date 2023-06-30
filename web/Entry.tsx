import React, {useMemo, useRef, useState} from "react";
import {
	Alert, Box,
	Checkbox,
	Divider,
	FormControlLabel,
	IconButton,
	LinearProgress,
	Popover,
	Stack,
	TextField
} from "@mui/material";

import Latex from "react-latex"

import AddIcon from '@mui/icons-material/Add';
import DeleteIcon from '@mui/icons-material/Delete';
import SettingsIcon from '@mui/icons-material/Settings';
import RefreshIcon from '@mui/icons-material/Refresh';
import {DragHandle} from "@mui/icons-material";

type Entry = {
	add: false,

	name?: string,
	ptr: any,
	value: string,
	len: number,
	tex_input?: string,
	output?: string[],
	error?: string,
	loading: boolean,
	autoupdate: boolean,
	output_step: number
}

type AddEntry = {
	add: true,

	name?: string,
	value: string,
	len: number,
	autoupdate: boolean
}

type TextEntry = {
	txt: string,
	delEnt: () => void
}

interface EntryProps {
	entry: Entry | AddEntry,
	setEnt: (x?: AddEntry) => void,

	addEnt?: (x: AddEntry) => void
}

interface OutputProps {
	entry: Entry
}

function GFOutput({entry}: OutputProps) {
	let gftex = useMemo(() => {
		console.log(entry.output);

		if (entry.output.length==1) return "0";

		let strout = "";
		let added=false;

		let offset = parseInt(entry.output[0]);

		for (let i=1,c=offset-1,imaginary=false; i<entry.output.length; i+=2,imaginary=!imaginary) {
			if (!imaginary) c++;

			if (entry.output[i]=="0") continue;

			let rest = entry.output[i];
			if (entry.output[i][0]=='-') strout += " - ", rest=entry.output[i].slice(1);
			else if (added) strout += " + ";

			if (imaginary) strout+="i";

			if (rest=="1" && entry.output[i+1]=="1") {
				if (c==0) {
					strout+=imaginary ? "" : "1";
				} else {
					strout+=c==1 ? "x" : `x^{${c}}`;
				}
			} else {
				let num = c==0 ? rest : (c==1 ? "x" : `x^{${c}}`);
				if (rest!="1" && c>0) num=`${rest}${num}`;
				if (entry.output[i+1]!="1") strout += `\\frac{${num}}{${entry.output[i+1]}}`;
				else strout+=num;
			}

			added=true;
		}

		if (entry.output.length>=2*entry.len+1) {
			strout += " + \\ldots";
		}

		return strout;
	}, [entry.output_step]);

	return (<Latex displayMode={false} >{`$$${entry.tex_input}=${gftex}$$`}</Latex>);
}

function TextEntry({txt,delEnt}: TextEntry) {
	return (<Stack spacing={1} direction="row" alignItems="center" >
		<DragHandle/>

		<Box sx={{flexGrow: 1}}>{txt}</Box>

		<Divider orientation="vertical" flexItem />
		<IconButton onClick={delEnt} ><DeleteIcon/></IconButton>
	</Stack>);
}

function Entry({entry, setEnt, addEnt}: EntryProps) {
	let [pentry, setPEntry] = useState({value: entry.name.length ? `${entry.name} = ${entry.value}` : entry.value, len: entry.len, autoupdate: entry.autoupdate});

	let nextUpdTimeout = useRef(null);

	let get_add_ent = (newpentry) => {
		let newent: AddEntry = {...newpentry, name: "", add: true};
		let i = newpentry.value.indexOf("=");

		if (i!=-1) {
			newent.name = newpentry.value.slice(0,i).trim();
			newent.value = newpentry.value.slice(i+1);
		}

		return newent;
	};

	let update_delayed = (newpentry, delay=true) => {
		if (nextUpdTimeout.current!=null) window.clearTimeout(nextUpdTimeout.current);
		setPEntry(newpentry);

		let newent = get_add_ent(newpentry);

		if (delay && newpentry.autoupdate) {
			nextUpdTimeout.current = window.setTimeout(() => {
				setEnt(newent);
			}, 300);
		} else if (!delay) {
			setEnt(newent);
		}
	};

	let [settingsOpen, setOpen] = useState(false);
	let settingsRef = useRef(null);

	let fst =
		(<Stack spacing={1} direction="row" alignItems="center" >
				{entry.add ? <></> : <DragHandle/>}

				<form action={"javascript:void(0);"} style={{width: "100%"}} onSubmit={() => {

					if (addEnt) {
						addEnt(get_add_ent(pentry));
						update_delayed({...pentry, value: ""}, false);
					} else {
						update_delayed(pentry, false);
					}
				}} >

					<TextField size={"small"} variant={"standard"} value={pentry.value} onChange={(e) => update_delayed({...pentry, value: e.target.value})} fullWidth={true} />
				</form>
				<Divider orientation="vertical" flexItem />
				{entry.add ? <></> : <IconButton onClick={() => setEnt(null)} ><DeleteIcon/></IconButton>}
				{(pentry.autoupdate ? <></> : <IconButton onClick={() => update_delayed(pentry, false)} ><RefreshIcon/></IconButton>)}
				<IconButton ref={settingsRef} onClick={() => setOpen(!settingsOpen)} ><SettingsIcon/></IconButton>

				<Popover open={settingsOpen}
								 anchorEl={settingsRef.current}
								 onClose={() => setOpen(false)}
								 anchorOrigin={{vertical: 'bottom', horizontal: 'left',}} >
					<Stack sx={{p: 3}} spacing={2} >
						<TextField size="small" label="Expansion length" type="number" value={pentry.len} onChange={(e) => {
							let num = parseInt(e.target.value);
							if (!isNaN(num)) update_delayed({...pentry, len: num})
						}} ></TextField>
						<FormControlLabel
							control={
								<Checkbox checked={pentry.autoupdate} onChange={(e, v) => update_delayed({...pentry, autoupdate: v})} name="autoupdate" />
							}

							label="Automatically update"
						/>
					</Stack>
				</Popover>

				{entry.add ? <IconButton onClick={() => addEnt && addEnt()} ><AddIcon/></IconButton> : <></>}
			</Stack>
		);

	let snd = <></>;

	if (!entry.add) {
		if (entry.loading) {
			snd = (<LinearProgress />);
		} else if (entry.error) {
			snd = (<Alert severity="error">{entry.error}</Alert>);
		} else if (entry.output) {
			snd = (<GFOutput entry={entry} ></GFOutput>);
		}
	}

	return (<Stack spacing={2} >{fst} {snd}</Stack>);
}

export {TextEntry, Entry, EntryProps, AddEntry};
