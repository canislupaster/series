import React, {useEffect, useState} from "react";
import {
	Box,
	Button,
	Card,
	CardActions,
	CardContent,
	Collapse,
	List,
	ListItem,
	ListItemButton,
	ListItemText,
	ListSubheader,
	Typography
} from "@mui/material";

import {ExpandLess, ExpandMore} from "@mui/icons-material";

import {DisplayEntry, NotebookData, makeNotebook} from "./Notebook";
import { AddEntry, emptyAddEnt } from "./Entry";

type IsOpen = {
	open: boolean,
	child: IsOpen[]
}

type DBEntry = {name: string, child: DBEntry[]} | {name: string, file: string, desc: string};

function DatabaseList({db, open, isOpen, setOpen, loading}: {db: DBEntry[], isOpen: IsOpen[], setOpen: (o: IsOpen[]) => void, open: (name: string,file: string) => void, loading: boolean}) {
	let list = db.map((entry,i) => {
			if ("file" in entry) {
				return (<ListItem key={entry.name} >
					<Card variant="outlined" sx={{width: "100%"}} >
						<CardContent>
							<Typography variant="h5" >{entry.name}</Typography>
							<Typography variant="body1" >{entry.desc}</Typography>
						</CardContent>
						<CardActions>
							<Button onClick={() => open(entry.name,entry.file)} >Open notebook</Button>
						</CardActions>
					</Card>
				</ListItem>);
			}

			return (<React.Fragment key={entry.name} >
				<ListItemButton onClick={() => {
					let l = [...isOpen]; l[i].open = !l[i].open;
					setOpen(l);
				}} >
					<ListItemText primary={entry.name} />
					{isOpen[i].open ? <ExpandLess /> : <ExpandMore />}
				</ListItemButton>
				<Collapse in={isOpen[i].open} unmountOnExit >
					<Box sx={{pl: 4}}>
						<DatabaseList db={entry.child} loading={loading} open={open} isOpen={isOpen[i].child}
							setOpen={(x) => {
								let l = [...isOpen];
								l[i].child = x;
								setOpen(l);
							}} />
					</Box>
				</Collapse>
			</React.Fragment>);
		});

	return (<List subheader={<ListSubheader></ListSubheader>} >
		{list}
	</List>);
}

function Database({open, nextId, CAS}: {open: (name: string, data: NotebookData) => void, nextId: React.MutableRefObject<number>, CAS: any}) {
	let [db,setDb] = useState<DBEntry[]>([]);
	let [isOpen,setIsOpen] = useState<IsOpen[]>([]);

	useEffect(() => {
		let open = window.localStorage.getItem("dbOpen");
		let newIsOpen = open ? JSON.parse(open) : [];

		import("./db/db.json").then((x) => {
			setDb(x);

			let makeIsOpen = (db: DBEntry[], isOpen: IsOpen[]) => {
				db.forEach((entry,i) => {
					if (i>=isOpen.length) isOpen.push({open: false, child: []});
					if ("child" in entry) makeIsOpen(entry.child,isOpen[i].child);
				});

				while (isOpen.length>db.length) isOpen.pop();
			};

			makeIsOpen(x, newIsOpen);
			setIsOpen(newIsOpen);
		}).catch(console.error);
	}, []);

	let [loading, setLoading] = useState(false);

	let db_open = (name,file) => {
		console.log(name);
		setLoading(true);
		fetch(`./db/${file}`)
			.then((resp) => resp.text())
			.then((x: string) => {
			let addEnt: AddEntry[]=[], dispEnt: DisplayEntry[]=[];
			while (true) {
				let i: number;
				let match = x.match(/(\d*)>\ *(?:(\w+)\ *=\ *)?([^=\n]+)/);
				if (match) i=match.index!; else i=x.length;

				let txt = x.slice(0,i).trim();
				if (txt.length>0) dispEnt.push({ty: "txt", id: nextId.current++, txt});

				console.log(match);
				if (match===null) break;

				x = x.slice(match.index!+match[0].length);

				dispEnt.push({ty: "ref", id: nextId.current++, to: addEnt.length});
				addEnt.push({...emptyAddEnt,
					name: match[2] || "", value: match[3], len: match[1] ? parseInt(match[1]) : 50
				});
			}

			open(name,makeNotebook(CAS, addEnt, dispEnt, nextId));
			setLoading(false);
		}).catch(console.error);
	};

	return (<DatabaseList isOpen={isOpen} setOpen={(x) => {
		window.localStorage.setItem("dbOpen",JSON.stringify(x));
		setIsOpen(x);
	}} loading={loading} db={db} open={db_open} ></DatabaseList>);
}

export {Database};