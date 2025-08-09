from inspect import isclass
from enum import Enum
from stat import S_IREAD, S_IRGRP, S_IROTH, S_IWUSR 

class record(object):
	def __init__(self, **kwds):
		self.__dict__.update(kwds)

	def __eq__(self, other):
		return self.__dict__ == other.__dict__

	def __str__(self):
		return str(self.__dict__)

	def __repr__(self):
		return str(self.__dict__)

	def __hash__(self):
		return hash(str(self))

class Term:
	def __init__(self, token):
		self.token = token

	def __eq__(self, other):
		return self.isTerminal() == other.isTerminal() and self.token == other.token

	#@functools.cache
	def __hash__(self):
		return hash(self.__str__()) 
	
	def __repr__(self):
		return Term.__str__(self)

	def to_str(self, terms, nonterms):
		return str(terms[self.token])

	def __str__(self):
		return '\'' + (('[' + str(self.token) + ']') if isinstance(self.token, int) else str(self.token)) + '\''

	def isTerminal(self):
		return True

class Nonterm:
	def __init__(self, value):
		self.value = value

	def __eq__(self, other):
		return self.isTerminal() == other.isTerminal() and  self.value == other.value

	#@functools.cache
	def __hash__(self):
		return hash(self.__str__()) 
	
	def __repr__(self):
		return Nonterm.__str__(self)

	def to_str(self, terms, nonterms):
		return nonterms[self.value].name

	def __str__(self):
		return ('[' + str(self.value) + ']') if isinstance(self.value, int) else str(self.value)

	def isTerminal(self):
		return False

class Prod:
	def __init__(self, name, body, operatorPrecedence=-1, action=None):
		self.name = name
		self.body = body
		self.operatorPrecedence = operatorPrecedence
		self.action = action

	def to_str(record, terms, nonterms):
		return f'{nonterms[record.nonterm].name} -> {", ".join(s.to_str(terms, nonterms) for s in record.body)}'

	def __str__(self):
		return self.name + '->' + ','.join(str(s) for s in self.body)
	
	def templateEnum(self, languageEnum):
		return Prod(self.name, 
			  [Term(languageEnum[symbol.token]) if symbol.isTerminal() else symbol for symbol in self.body], 
			  self.operatorPrecedence, self.action)

class LRActionType(Enum):
	SHIFT = 0
	REDUCE = 1
	ERROR = 2
	ACCEPT = 3

LRActionTypeMask = 0x0003
LRActionTypeShift = 0
LRActionValueMask = 0xFFFC
LRActionValueShift = 2

class LRAction:
	def __init__(self, action, value=0):
		self.action = action
		self.value = value

	def shiftState(self):
		if self.action != LRActionType.SHIFT:
			raise Exception()
		return self.value

	def reduceProduction(self):
		if self.action != LRActionType.REDUCE:
			raise Exception()
		return self.value
	
	def toUInt16(self):
		return self.action.value + (self.value << LRActionValueShift)

	def __str__(self):
		if self.action == LRActionType.SHIFT:
			return 's' + str(self.shiftState())
		if self.action == LRActionType.REDUCE:
			return 'r' + str(self.reduceProduction())
		if self.action == LRActionType.ERROR:
			return 'e'
		if self.action == LRActionType.ACCEPT:
			return 'acc'
		return None

	def __eq__(self, other):
		return str(self) == str(other)

def shift(nextState):
	return LRAction(LRActionType.SHIFT, nextState)

def reduce(production):
	return LRAction(LRActionType.REDUCE, production)

def accept():
	return LRAction(LRActionType.ACCEPT)

def error():
	return LRAction(LRActionType.ERROR)

class Model:
	def __init__(self, productions, language, operatorsAssociativity, startProductionName=None):
		if isclass(language) and issubclass(language, Enum) or isinstance(language, Enum):
			languageEnum = language
			language = list(languageEnum)
			productions = [production.templateEnum(languageEnum) for production in productions]

		if startProductionName is None:
			startProductionName = productions[0].name
		augmentedProduction = Prod('\'' + startProductionName, [Nonterm(startProductionName)])
		productions = [augmentedProduction] + productions

		self.operatorsAssociativity = operatorsAssociativity

		self.__startProductionName = augmentedProduction.name
		self.__constructSymbols(productions, language)

		self.__constructFirstTerminals()
		self.__constructKernelItems()

		self.__constructActionTable()

		#self.__printFirsts()
		#print(*(f'{index} {nonterm.name}' for index, nonterm in enumerate(self.__nonterms)), sep='\n')
		#self.__printKernels()
		#self.__printActionTable()

	def __printFirsts(self):
		for nonterm in self.__nonterms:
			print(f'{nonterm.name}->{"".join(self.__termToStr(term) for term in nonterm.firstTerms)}')

	def __printKernels(self):
		for index, kernelItems in enumerate(self.__kernelItemSets):
			print(index)
			for item, lookaheads in self.__closure(kernelItems.items).items():
				production = ''
				production += '\t'
				production += self.__nonterms[item.nonterm].name
				production += '->'
				production += ''.join(self.__symbolsToStr(self.__productions[item.prodIndex].body[:item.dotPos]))
				production += '.'
				production += ''.join(self.__symbolsToStr(self.__productions[item.prodIndex].body[item.dotPos:]))
				production += '\t'
				production += '/'.join(self.__termToStr(lookahead) for lookahead in lookaheads)
				print(production)
			for symbol, targetState in kernelItems.goto.items():
				print(f'\tgoto by {self.__symbolToStr(symbol)} to {targetState}')

	def __printActionTable(self):
		symbols = []
		symbols.extend(Term(index) for index in range(len(self.__terms) + 1))
		symbols.extend(Nonterm(index) for index in range(1, len(self.__nonterms)))
		
		print('', *self.__symbolsToStr(symbols), sep='\t')
		for state, symbolActions in enumerate(self.__actionTable):
			infoLine = str(state)
			for index in range(len(self.__terms) + 1):
				infoLine += '\t'
				infoLine += str(symbolActions.actions[index])
			for index in range(1, len(self.__nonterms)):
				infoLine += '\t'
				goto = symbolActions.goto[index]
				infoLine += str(goto) if goto >= 0 else '-'
			print(infoLine)

	def __first(self, symbols):
		first = set()

		for symbol in symbols:
			if symbol.isTerminal():
				first.add(symbol.token)
				break

			firstOfSymbol = self.__nonterms[symbol.value].firstTerms
			first.update(firstOfSymbol)

			if self.__getEmptyTermIndex() in first:
				first.remove(self.__getEmptyTermIndex())
			else:
				break
		else:
			first.add(self.__getEmptyTermIndex())

		return first

	def __closure(self, itemSetState):
		closure = {}
		iterClosure = dict(itemSetState)

		while len(iterClosure) > 0:
			currentIter = iterClosure.copy()
			iterClosure.clear()

			for item, lookaheads in currentIter.items():
				self.__mergeItemIntoSet(closure, item, lookaheads)

				symbols = self.__productions[item.prodIndex].body[item.dotPos:]
			
				if len(symbols) == 0 or symbols[0].isTerminal():
					continue

				nonterm = symbols[0].value
				for productionIndex in self.__nonterms[nonterm].bodyIndices:
					for nextToken in lookaheads:
						firstAfterSet = self.__first(symbols[1:] + [Term(nextToken)])
						for firstAfter in firstAfterSet:
							newItem = Model.__it(nonterm, productionIndex)
							if not newItem in closure or not firstAfter in closure[newItem]:
								self.__mergeItemIntoSet(iterClosure, newItem, [firstAfter])

		return closure

	def __goto(self, itemSetState, symbol):
		goto = {}

		for item, lookaheads in itemSetState.items():
			symbols = self.__productions[item.prodIndex].body[item.dotPos:]
			if len(symbols) == 0 or symbols[0] != symbol:
				continue

			newItem = Model.__it(item.nonterm, item.prodIndex, dotPos=item.dotPos + 1)
			self.__mergeItemIntoSet(goto, newItem, lookaheads)

		return goto

	def __getStartItemSet(self):
		return frozenset(Model.__it(0, productionBodyIndex) for productionBodyIndex in self.__nonterms[0].bodyIndices)

	def __getErrorTermIndex(self):
		return len(self.__terms) + 1

	def __getEmptyTermIndex(self):
		return len(self.__terms) + 2

	def __getEndTermIndex(self):
		return len(self.__terms)

	def __symbolsToStr(self, symbols):
		return (self.__symbolToStr(symbol) for symbol in symbols)

	def __symbolToStr(self, symbol):
		if symbol.isTerminal():
			return self.__termToStr(symbol.token)
		else:
			return self.__nontermToStr(symbol.value)

	def __nontermToStr(self, nonterm):
		return self.__nonterms[nonterm].name# + f'[{symbol.value}]'

	def __termToStr(self, token):
		if token == self.__getEndTermIndex():
			return '$'# + f'[{token}]'
		elif token == self.__getErrorTermIndex():
			return 'Err'
		elif token == self.__getEmptyTermIndex():
			return '_'
		else:
			if token >= len(self.__terms):
				print(f'error! token = {token}')
			return f'\'{self.__terms[token]}\''# + f'[{token}]'

	def __lookaheadsToStr(self, tokens):
		return '/' if len(tokens) == 0 else '/'.join(self.__termToStr(token) for token in tokens) 

	def __itemSetToStr(self, itemSet):
		return '{}' if len(itemSet) == 0 else f'{{{", ".join(self.__itemToStr(itemPair) for itemPair in itemSet.items())}}}'

	def __itemToStr(self, itemPair):
		item, lookaheads = itemPair
		symbols = self.__productions[item.prodIndex].body
		return f'{self.__nontermToStr(item.nonterm)}->{"".join(self.__symbolsToStr(symbols[:item.dotPos]))}.{"".join(self.__symbolsToStr(symbols[item.dotPos:]))}\t{self.__lookaheadsToStr(lookaheads)}'

	def __it(nonterm, prodIndex, dotPos=0):
		return record(nonterm=nonterm, prodIndex=prodIndex, dotPos=dotPos)

	def __constructSymbols(self, productions, language):
		self.__productions = []
		self.__nonterms = []
		nontermsIndices = {}

		firstProduction = productions[0]
		self.__registerNontermSymbol(firstProduction.name,  nontermsIndices)

		self.__terms = list(language)
		termsIndices = {}
		for index, term in enumerate(self.__terms):
			termsIndices[term] = index

		termsIndices[None] = self.__getErrorTermIndex()

		for production in productions:
			self.__registerNontermSymbol(production.name, nontermsIndices)

			for symbol in production.body:
				if not symbol.isTerminal():
					self.__registerNontermSymbol(symbol.value, nontermsIndices)

			body = [(Term(termsIndices[symbol.token]) if symbol.isTerminal() else Nonterm(nontermsIndices[symbol.value])) for symbol in production.body]

			nonterm = nontermsIndices[production.name]
			self.__nonterms[nonterm].bodyIndices.add(len(self.__productions))
			self.__productions.append(record(nonterm=nonterm, body=body, operatorPrecedence=production.operatorPrecedence, action=production.action))

	def __registerNontermSymbol(self, name, nontermDict):
		if not name in nontermDict:
			nontermDict[name] = len(self.__nonterms)
			self.__nonterms.append(record(name = name, bodyIndices = set(), firstTerms = set() ))

	def __constructFirstTerminals(self):
		updated = True
		while updated:
			updated = False

			for nonterm in self.__nonterms:
				productionIndices = nonterm.bodyIndices
				for productionIndex in productionIndices:
					firstTerminals = nonterm.firstTerms

					productionBody = self.__productions[productionIndex].body

					for symbol in productionBody:
						if symbol.isTerminal():
							if not symbol.token in firstTerminals:
								firstTerminals.add(symbol.token)
								updated = True 
							break

						firstOfSymbol = self.__nonterms[symbol.value].firstTerms
						for iterFirst in firstOfSymbol:
							if iterFirst != self.__getEmptyTermIndex() and not iterFirst in firstTerminals:
								firstTerminals.add(iterFirst)
								updated = True 

						if not self.__getEmptyTermIndex() in firstOfSymbol:
							break
					else:
						if not self.__getEmptyTermIndex() in firstTerminals:
							firstTerminals.add(self.__getEmptyTermIndex())
							updated = True


	def __constructKernelItems(self):
		symbols = [Nonterm(index) for index in range(len(self.__nonterms))] + [Term(index) for index in range(len(self.__terms))]

		initialKernelItemSet = self.__getStartItemSet()

		self.__kernelItemSets = [record( items = self.__closure({ item: set([self.__getEndTermIndex()]) for item in initialKernelItemSet }), goto = {} )]
		kernelItemSetsIndices = {initialKernelItemSet: 0}

		#print(f'init state {0}: {self.__itemSetToStr(self.__kernelItemSets[0].items)}')

		iteration = set([ 0 ])

		while len(iteration) > 0:
			#print('ITERATION')

			currentIter = set(iteration)
			iteration.clear()

			for currentKernelIndex in currentIter:
				for symbol in symbols:
					kernelItemSet = self.__kernelItemSets[currentKernelIndex]

					gotoLookaheadSet = self.__goto(kernelItemSet.items, symbol)
					if len(gotoLookaheadSet) == 0:
						continue

					gotoItemSet = frozenset(gotoLookaheadSet)
					gotoClosure = self.__closure(gotoLookaheadSet)

					if not gotoItemSet in kernelItemSetsIndices:
						gotoKernelIndex = len(self.__kernelItemSets)
						kernelItemSet.goto[symbol] = gotoKernelIndex
						iteration.add(gotoKernelIndex)

						kernelItemSetsIndices[gotoItemSet] = gotoKernelIndex
						self.__kernelItemSets.append(record( items = gotoClosure, goto = {} ))
						#print(f'new state {gotoKernelIndex}: {self.__itemSetToStr(gotoClosure)}')
						continue

					gotoKernelIndex = kernelItemSetsIndices[gotoItemSet]
					kernelItemSet.goto[symbol] = gotoKernelIndex

					gotoKernelItemSet = self.__kernelItemSets[gotoKernelIndex]
					for item, lookaheads in gotoClosure.items():
						for lookahead in lookaheads:
							if not lookahead in gotoKernelItemSet.items[item]:
								iteration.add(gotoKernelIndex)
								#print(f'state {gotoKernelIndex}: new {item} : {self.__termToStr(lookahead)}')
								break

						gotoKernelItemSet.items[item].update(lookaheads)

			#self.__printKernels()

	def __mergeItemIntoSet(self, dItemSet, item, lookaheads):
		if item in dItemSet:
			dItemSet[item].update(lookaheads)
		else:
			dItemSet[item] = set(lookaheads)

	def __setActionTableValue(self, state, item, symbol, action):
		if symbol.isTerminal():
			self.__setActionTableAction(state, item, symbol.token, action)
		else:
			self.__setActionTableGoto(state, item, symbol.value, action)

	def __setActionTableAction(self, state, item, term, action):
		if term >= len(self.__actionTable[state].actions):
			print(f'state={state}, item={item}. term={term}, action={action}')
		currentAction = self.__actionTable[state].actions[term]

		if currentAction.action == LRActionType.ERROR or action.action == LRActionType.ACCEPT:
			self.__actionTable[state].actions[term] = action
			return

		if currentAction == action:
			return

		if action.action == currentAction.action:
			self.__actionTableError(state, Term(term), action, currentAction)
			return

		shiftAction = error()
		reduceAction = error()
		if currentAction.action == LRActionType.SHIFT:
			shiftAction = currentAction
			reduceAction = action
		else:
			shiftAction = action
			reduceAction = currentAction

		prodIndex = reduceAction.reduceProduction()
		leftOperatorPrecedence = self.__productions[prodIndex].operatorPrecedence

		targetSetIndex = shiftAction.shiftState()
		rightOperatorPrecedence = -1

		for item in self.__kernelItemSets[targetSetIndex].items:
			if item.dotPos > 0:
				proposedOperatorPrecedence = self.__productions[item.prodIndex].operatorPrecedence
				if proposedOperatorPrecedence >= 0:
					if rightOperatorPrecedence >= 0:
						self.__actionTableError(state, Term(term), action, currentAction)
						return
					rightOperatorPrecedence = proposedOperatorPrecedence

		if leftOperatorPrecedence < 0 or rightOperatorPrecedence < 0:
			if currentAction.action == LRActionType.SHIFT:
				return

		elif leftOperatorPrecedence < rightOperatorPrecedence:
			if currentAction.action == LRActionType.REDUCE:
				return

		elif leftOperatorPrecedence > rightOperatorPrecedence:
			if currentAction.action == LRActionType.SHIFT:
				return

		else:
			if leftOperatorPrecedence >= len(self.operatorsAssociativity):
				print(f'leftOperatorPrecedence={leftOperatorPrecedence}, self.operatorsAssociativity={self.operatorsAssociativity}')
			associativity = self.operatorsAssociativity[leftOperatorPrecedence]
			if associativity == 'ltr' and currentAction.action == LRActionType.REDUCE or associativity == 'rtl' and currentAction.action == LRActionType.SHIFT:
				return

		self.__actionTable[state].actions[term] = action

	def __setActionTableGoto(self, state, item, nonterm, action):
		currentGoto = self.__actionTable[state].goto[nonterm]

		if action.action != LRActionType.SHIFT:
			self.__actionTableError(state, Nonterm(nonterm), action, currentGoto)
			return

		if currentGoto < 0:
			self.__actionTable[state].goto[nonterm] = action.shiftState()
			return

		if currentGoto == action.shiftState():
			return

		self.__actionTableError(state, Nonterm(nonterm), action, currentGoto)

	def __actionTableError(self, state, symbol, action, currentAction):
			print('Failed to set ' + str(action) + ' to table in ' + str(state) + ':' + self.__symbolToStr(symbol) + '; already filled with ' + str(currentAction))
			if action.action == LRActionType.REDUCE:
				print(f'{action} = {Prod.to_str(self.__productions[action.value], self.__terms, self.__nonterms)}')
			if currentAction.action == LRActionType.REDUCE:
				print(f'{currentAction} = {Prod.to_str(self.__productions[currentAction.value], self.__terms, self.__nonterms)}')

	def __constructActionTable(self):
		self.__actionTable = [record(actions=[error()] * (len(self.__terms) + 2), goto=[-1] * len(self.__nonterms)) for _ in range(len(self.__kernelItemSets))]

		for state, kernel in enumerate(self.__kernelItemSets):
			for item, lookaheads in kernel.items.items():
				symbolsAfterDot = self.__productions[item.prodIndex].body[item.dotPos:]

				if len(symbolsAfterDot) == 0:
					if item.nonterm != 0:
						for lookahead in lookaheads:
							self.__setActionTableValue(state, item, Term(lookahead), reduce(item.prodIndex))

					elif self.__getEndTermIndex() in lookaheads:
						self.__setActionTableValue(state, item, Term(self.__getEndTermIndex()), accept())

					continue

				firstSymbolAfterDot = symbolsAfterDot[0]
				self.__setActionTableValue(state, item, firstSymbolAfterDot, shift(kernel.goto[firstSymbolAfterDot]))

	def buildParser(self, getTokenIndex):
		return Parser([[reduce(action.reduceProduction() - 1) if action.action == LRActionType.REDUCE else action for action in table.actions] for table in self.__actionTable]
			, [table.goto[1:].copy() for table in self.__actionTable]
			, [ record(nonterm=production.nonterm - 1, length=len(production.body), action=production.action) for production in self.__productions[1:] ]
			, self.__getEndTermIndex()
			, self.__getErrorTermIndex()
			, getTokenIndex)

	def parse(self, getTokenType, tokens, userdata):
		return self.buildParser(getTokenType).parse(tokens, userdata)
	
	def formatTable(table):
		lines = []
		for row in table:
			line = ''
			for value in row:
				line += f'{value},\t'
			lines += [line]
		return '\n    '.join(lines)
	
	def getCTemplateMapping(self):
		actionTableBody = Model.formatTable([[(reduce(action.reduceProduction() - 1) if action.action == LRActionType.REDUCE else action).toUInt16() for action in table.actions] for table in self.__actionTable])
		gotoTableBody = Model.formatTable([table.goto[1:].copy() for table in self.__actionTable])

		prodNontermsArrayBody = ''
		prodLengthsArrayBody = ''
		for production in self.__productions[1:]:
			prodNontermsArrayBody += f'{production.nonterm - 1},\t'
			prodLengthsArrayBody += f'{len(production.body)},\t'

		productionActionsCasesList = []
		productionActionIndent = '\n' + ' ' * 4 * 5
		for i, production in enumerate(self.__productions[1:]):
			if production.action is not None:
				formattedAction = production.action.replace('\n', productionActionIndent)
				caseLines = [
					f'case {i}: {{',
					f'{formattedAction}',
					'break;',
					'}',
				]
				productionActionsCasesList += caseLines
		productionActionsCases = productionActionIndent.join(productionActionsCasesList)
		
		return { 
			'actionTableBody' : actionTableBody,
			'gotoTableBody' : gotoTableBody,
			'nontermCount' : len(self.__nonterms) - 1, 
			'prodNontermsArrayBody' : prodNontermsArrayBody,
			'prodLengthsArrayBody' : prodLengthsArrayBody,
			'productionActionsSwitch' : productionActionsCases,
			}

class Parser:
	def __init__(self, actions, goto, productionInfos, endTokenIndex, errorTokenIndex, getTokenIndex):
		self.actions = actions
		self.goto = goto
		self.productionInfos = productionInfos
		self.endTokenIndex = endTokenIndex
		self.errorTokenIndex = errorTokenIndex
		self.getTokenIndex = getTokenIndex

	def parse(self, tokens, userdata):
		stack = [record(index=0, attr=None)]

		for token in (tokens + [None]):
			tokenIndex = self.endTokenIndex if token is None else self.getTokenIndex(token)
			print(f"parsing token {tokenIndex}")
			while True:
				action = self.actions[stack[-1].index][tokenIndex]

				print(f'from state {stack[-1].index} and term {tokenIndex}')
				print(f'actionCode {action.toUInt16()}, action {action.action.value}')
				if action.action == LRActionType.SHIFT:
					stack.append(record(index=action.shiftState(), attr=token))
					print(f'pushing {stack[-1].index}')
					print(*(state.index for state in stack))
					break
				if action.action == LRActionType.REDUCE:
					prodIndex = action.reduceProduction()
					productionInfo = self.productionInfos[prodIndex]
					prodActionEval = productionInfo.action
					result = None
					if prodActionEval is not None:
						result = eval(prodActionEval)(stack, userdata)
					for _ in range(productionInfo.length):
						stack.pop()
					print(f'reducing {productionInfo.length}')
					print(*(state.index for state in stack))
					print(f'from state {stack[-1].index} and nonterm {productionInfo.nonterm}')
					stack.append(record(index=self.goto[stack[-1].index][productionInfo.nonterm], attr=result))
					print(f'pushing {stack[-1].index}')
					print(*(state.index for state in stack))
					if stack[-1].index < 0:
						print(f'uknown goto by nonterm {productionInfo.nonterm} of length {productionInfo.length}!')
				if action.action == LRActionType.ERROR:
					print(f'can\'t process {token}!')
					print(*(state.index for state in stack))
					return
				if action.action == LRActionType.ACCEPT:
					return stack[-1].attr

		print('no more tokens!')
		print(*(state.index for state in stack))



	