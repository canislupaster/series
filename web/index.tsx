import React, {useEffect, useMemo, useRef, useState} from 'react';
import ReactDOM from 'react-dom/client';
import CASModule from "./cas.mjs";

import '@fontsource/roboto/300.css';
import '@fontsource/roboto/400.css';
import '@fontsource/roboto/500.css';
import '@fontsource/roboto/700.css';

import {
	Alert,
	Box,
	Button,
	Checkbox,
	Container,
	createTheme,
	CssBaseline,
	Divider,
	FormControlLabel,
	IconButton,
	Paper,
	Popover,
	Slider,
	Stack,
	Tabs,
	Tab,
	TextField,
	ThemeProvider,
	Typography,
	InputBase,
	Dialog,
	DialogTitle,
	DialogContent,
	DialogContentText,
	DialogActions,
	useTheme,
	CircularProgress
} from "@mui/material";

import {Notebook, NotebookData, delNotebook, makeNotebook, DisplayEntry} from "./Notebook";
import {AddEntry, Entry} from "./Entry";
import AddIcon from "@mui/icons-material/Add";
import DeleteIcon from "@mui/icons-material/Delete";
import {DragStack} from "./DragStack";
import {Close} from "@mui/icons-material";
import {Database} from "./Database";

type Tab = {
	name: string,
	id: number,
	addEntries: AddEntry[],
	dispEntries: DisplayEntry[],
	data?: NotebookData
};

function App({CAS}: {CAS: any}) {
	let idNum = useRef(0);
	let [tabs, setTabs] = useState<Tab[]>([]);
	let tabsRef = useRef(tabs);

	let [tab, setTabRaw] = useState(-1);

	useEffect(() => {
		let new_tabs: Tab[] = [];

		let tablist = window.localStorage.getItem("tabs");
		if (tablist) {
			for (let tab of tablist.split("\n")) {
				let tab_data = window.localStorage.getItem(`tab-${tab}`);
				if (!tab_data) continue;

				let tdata = JSON.parse(tab_data), tid=parseInt(tab);

				//ensure no collisions... im so sorry :( should just clean this up w/ separate refs but that'd take more code than there is here
				if (tid>=idNum.current) idNum.current=tid+1;

				new_tabs.push({name: tdata.name, id: tid, addEntries: tdata.entries, dispEntries: tdata.dispEntries});
			}
		} else {
			// new_tabs.push({name: "Untitled", id: idNum.current++, data: makeNotebook(CAS, [], idNum)});
		}

		setTabs(new_tabs);
		let cur = parseInt(window.localStorage.getItem("current"));
		if (!isNaN(cur)) setTab(cur);

		return () => {
			for (let tab of tabsRef.current) delNotebook(tab.data);
		};
	}, []);

	let updateTabs = (newTabs) => {
		setTabs(newTabs); tabsRef.current = newTabs;
		window.localStorage.setItem("tabs", newTabs.map((x) => x.id).join("\n"));
	};

	let setTab = (i) => {
		setTabRaw(i);
		window.localStorage.setItem("current", `${i}`);
	};

	let updateTab = (idx, newTab) => {
		let newTabs = [...tabs];
		newTabs[idx] = newTab;
		updateTabs(newTabs);

		window.localStorage.setItem(`tab-${newTab.id}`, JSON.stringify({
			name: newTab.name,
			entries: newTab.data.entries.map((x) => {return {
				name: x.name, value: x.value, len: x.len, autoupdate: x.autoupdate
			}}),
			dispEntries: newTab.data.dispEntries
		}));
	};

	useEffect(() => {
		if (tabs.length>tab && tab>=0 && tabs[tab].data==null) {
			updateTab(tab, {...tabs[tab], data: makeNotebook(CAS, tabs[tab].addEntries, tabs[tab].dispEntries, idNum)});
		}
	}, [tab, tabs]);

	let [deletion, setDeletion] = useState<null | number>(null);
	let do_delete = (del) => () => {
		if (del) {
			delNotebook(tabs[deletion].data);
			let newTabs = [...tabs];
			newTabs.splice(deletion, 1);
			updateTabs(newTabs);

			if (tab>=deletion) setTab(tab-1);
		}

		setDeletion(null);
	};

	const theme = useTheme();

	if (tabs.length<=tab) return <></>

	let tab_box = (inner, i) => {
		return (<Box onClick={() => {
			if (i!=tab) setTab(i);
		}} sx={{
				backgroundColor: i==tab ? theme.palette.primary.dark : theme.palette.background.default,
				px: 1,
				borderRadius: "10px 10px 0 0",
				cursor: "pointer"
		}} >{inner}</Box>);
	};

	let tab_els = tabs.map((x, i) => {
		let inner = (<InputBase disabled={i!=tab} value={x.name} onChange={(e) => {
				updateTab(i, {...x, name: e.target.value});
			}} ></InputBase>);

		if (i==tab) inner = (<>
			{inner}
			<IconButton size={"small"} onClick={() => setDeletion(i)} >
				<Close fontSize={"small"} />
			</IconButton>
		</>);

		return {key:x.id, el: tab_box(inner, i)};
	});

	tab_els.unshift({key: "home", el: tab_box(<InputBase disabled={tab!=-1} value="Home" readOnly />, -1)});

	let cur_tab = <CircularProgress />;
	if (tab>=0 && tabs[tab].data) {
		cur_tab = <Notebook CAS={CAS} data={tabs[tab].data} setData={(data) => updateTab(tab, {...tabs[tab], data})} />;
	} else if (tab==-1) {
		cur_tab = (<>
			<Typography variant={"h4"} >the generatingfunctionologist's playground</Typography>
			<Database open={(name,data) => {
				setTab(tabs.length);
				updateTabs([...tabs, {name, id: idNum.current++, data}]);
			}} nextId={idNum} CAS={CAS} ></Database>
		</>);
	}

	return (
		<Paper>
			<Dialog
        open={deletion!=null}
        onClose={do_delete(false)}
        aria-labelledby="alert-dialog-title"
        aria-describedby="alert-dialog-description"
      >
        <DialogTitle id="alert-dialog-title">
					Delete notebook {tabs[deletion]?.name}?
        </DialogTitle>
        <DialogActions>
          <Button onClick={do_delete(false)} >Cancel</Button>
          <Button onClick={do_delete(true)} >Delete</Button>
        </DialogActions>
      </Dialog>

			<Container maxWidth="md" sx={{py: 3}} >
				<Stack direction={"row"} >
					<DragStack vertical={true} direction={"row"} alignItems={"end"} elements={tab_els} moveElement={(i,to) => {
						if (i==0 || to==0) return;
						i--, to--;

						let arr_cpy = [...tabs];
						let [removed] = arr_cpy.splice(i,1);
						arr_cpy.splice(to,0,removed);

						if (i==tab) setTab(to);

						updateTabs(arr_cpy);
					}} >
					</DragStack>

					<IconButton size={"small"} onClick={() => {
						setTab(tabs.length);
						updateTabs([...tabs, {name: "Untitled", id: idNum.current++, data: makeNotebook(CAS, [], [], idNum)}]);
					}} ><AddIcon/></IconButton>
				</Stack>

				<Box sx={{py: 3}}>
					{cur_tab}
				</Box>
			</Container>
		</Paper>
	);
}

CASModule().then((cas) => {
	const darkTheme = createTheme({
		palette: {
			mode: 'dark',
		},
	});

	let init = () => {
		const root = ReactDOM.createRoot(
			document.getElementById('root') as HTMLElement
		);

		root.render(
		<React.StrictMode>
			<ThemeProvider theme={darkTheme}>
				<CssBaseline enableColorScheme />
				<App CAS={cas} />
			</ThemeProvider>
		</React.StrictMode>
		);
	};

	if (document.readyState=="complete") init();
	else window.onload = init;
});
