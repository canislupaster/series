import React, {useEffect, useMemo, useRef, useState} from "react";
import {
	Alert, Box,
	Checkbox,
	Divider,
	FormControl,
	FormControlLabel,
	IconButton,
	InputLabel,
	LinearProgress,
	MenuItem,
	Popover,
	Select,
	Stack,
	TextField
} from "@mui/material";

import Latex from "react-latex"

import AddIcon from '@mui/icons-material/Add';
import DeleteIcon from '@mui/icons-material/Delete';
import SettingsIcon from '@mui/icons-material/Settings';
import RefreshIcon from '@mui/icons-material/Refresh';
import {DragHandle, StreamOutlined} from "@mui/icons-material";

type AddEntry = {
	name: string,
	value: string,
	len: number,
	autoupdate: boolean,
	output_kind: "OGF" | "EGF" | "Float",
	float_prec: number
}

type Entry = AddEntry & {
	ptr: any,
	tex_input?: string,
	output?: string[],
	error?: string,
	loading: boolean,
	output_step: number
}

const emptyAddEnt: AddEntry = {
	name: "", value: "", len: 50, autoupdate: true,
	output_kind: "OGF", float_prec: 53
};

const save_keys = Object.keys(emptyAddEnt);

let toAddEnt = (x: AddEntry): AddEntry => {
	let o: any = {};
	for (let k of save_keys) o[k]=x[k];
	return o;
};

type TextEntry = {
	txt: string,
	delEnt: () => void,
	setDraggable: (d: boolean) => void
}

interface EntryProps {
	entry: ({add: false} & Entry) | ({add: true} & AddEntry),
	setEnt: (x?: AddEntry) => void,

	addEnt?: (x: AddEntry) => void,
	setDraggable: (d: boolean) => void
}

interface OutputProps {
	entry: Entry
}

function GFOutput({entry}: OutputProps) {
	let gftex = useMemo(() => {
		if (!entry.output) return "";
		console.log(entry.output);

		if (entry.output.length==1) return "0";

		let strout = "";
		let added=false;

		let offset = parseInt(entry.output[0]);

		for (let i=1,c=offset-1,imaginary=false; i<entry.output.length; i+=2,imaginary=!imaginary) {
			if (!imaginary) c++;

			if (entry.output[i].length==0 || entry.output[i]=="0") continue;

			let rest = entry.output[i];
			if (entry.output[i][0]=='-') strout += " - ", rest=entry.output[i].slice(1);
			else if (added) strout += " + ";

			if (imaginary) strout+="i";

			let num = c==0 ? (imaginary ? "" : rest) : (c==1 ? "x" : `x^{${c}}`);
			if (rest!="1" && c>0) num=`${rest}${num}`;

			if (entry.output_kind=="EGF") {
				let f = c<0 ? `(${c})!` : `${c}!`;
				if (entry.output[i+1]!="1") f += `\\cdot ${entry.output[i+1]}`;
				strout += `\\frac{${num}}{${f}}`;
			} else if (entry.output[i+1]!="1" && entry.output_kind!="Float") {
				strout += `\\frac{${num}}{${entry.output[i+1]}}`;
			} else if (entry.output[i+1]!="0" && entry.output_kind=="Float") {
				strout += `${num} \\times 10^{${entry.output[i+1]}}`;
			} else {
				strout+=num;
			}

			added=true;
		}

		if (entry.output.length>=2*entry.len+1) {
			strout += " + \\ldots";
		}

		return strout;
	}, [entry.output_step]);

	if (gftex.length>1e4) return (<>
		<Alert severity="info">you think you're so smart with your gargantuan GF? i wont fancy-format it this time</Alert>
		<code>{`${entry.tex_input}=${gftex}`}</code>
	</>);
	return (<Latex displayMode={false} >{`$$${entry.tex_input}=${gftex}$$`}</Latex>);
}

function Drag({setDraggable}: {setDraggable: (d: boolean) => void}) {
	let dragHandleRef = useRef<SVGSVGElement>(null);

	useEffect(() => {
		if (dragHandleRef.current) {
			let listen = (ev: MouseEvent) => setDraggable(ev.buttons!=0);

			let elem = dragHandleRef.current;
			elem.addEventListener("mousedown", listen);
			window.addEventListener("mouseup", listen);

			return () => {
				elem.removeEventListener("mousedown", listen);
				window.removeEventListener("mouseup", listen);
			};
		}
	}, [dragHandleRef.current]);

	return <DragHandle ref={dragHandleRef} />;
}

function TextEntry({txt,delEnt,setDraggable}: TextEntry) {
	return (<Stack spacing={1} direction="row" alignItems="center" >
		<Drag setDraggable={setDraggable} />

		<Box sx={{flexGrow: 1}}>{txt}</Box>

		<Divider orientation="vertical" flexItem />
		<IconButton onClick={delEnt} ><DeleteIcon/></IconButton>
	</Stack>);
}

function Entry({entry, setEnt, addEnt, setDraggable}: EntryProps) {
	let [pentry, setPEntry] = useState<AddEntry>({...toAddEnt(entry), value: entry.name?.length ? `${entry.name} = ${entry.value}` : entry.value});

	let nextUpdTimeout = useRef<null|number>(null);

	let get_add_ent = (newpentry) => {
		let newent: AddEntry = {...newpentry, name: ""};
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

	let num_field = (v: number,up: (x: number) => void,label: string) => {
		return <TextField size="small" label={label} type="number" value={v} onChange={(e) => {
			let num = parseInt(e.target.value);
			if (!isNaN(num)) up(num);
		}} ></TextField>;
	};

	let fst =
		(<Stack spacing={1} direction="row" alignItems="center" >
				{addEnt ? <></> : <Drag setDraggable={setDraggable} />}

				<form style={{width: "100%"}} onSubmit={(e) => {
					e.preventDefault();

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
				{addEnt ? <></> : <IconButton onClick={() => setEnt(undefined)} ><DeleteIcon/></IconButton>}
				{(pentry.autoupdate ? <></> : <IconButton onClick={() => update_delayed(pentry, false)} ><RefreshIcon/></IconButton>)}
				<IconButton ref={settingsRef} onClick={() => setOpen(!settingsOpen)} ><SettingsIcon/></IconButton>

				<Popover open={settingsOpen}
								 anchorEl={settingsRef.current}
								 onClose={() => setOpen(false)}
								 anchorOrigin={{vertical: 'bottom', horizontal: 'left',}} >
					<Stack sx={{p: 3}} spacing={2} >
						{num_field(pentry.len, (num) => update_delayed({...pentry, len: num}), "Expansion length")}

						<FormControlLabel
							control={
								<Checkbox checked={pentry.autoupdate} onChange={(e, v) => update_delayed({...pentry, autoupdate: v})} name="autoupdate" />
							}

							label="Automatically update"
						/>

						<FormControl>
							<InputLabel>Output as</InputLabel>
							<Select
								value={pentry.output_kind}
								label="Output as"
								onChange={(ev) => update_delayed({...pentry, output_kind: ev.target.value})}
							>
								<MenuItem value={"OGF"}>Ordinary</MenuItem>
								<MenuItem value={"EGF"}>Exponential</MenuItem>
								<MenuItem value={"Float"}>Floating-point</MenuItem>
							</Select>
						</FormControl>

						{pentry.output_kind=="Float" ? num_field(pentry.float_prec, (num) => update_delayed({...pentry, float_prec: num}), "Floating-point precision") : <></>}
					</Stack>
				</Popover>

				{addEnt ? <IconButton onClick={() => addEnt(get_add_ent(pentry))} ><AddIcon/></IconButton> : <></>}
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

export {TextEntry, Entry, EntryProps, AddEntry, emptyAddEnt, toAddEnt};
