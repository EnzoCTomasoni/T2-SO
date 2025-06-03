#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <list>
#include <vector>
#include <bitset>
#include <string>

#define TAMANHO_PAGINA 4096
#define BITS_DESLOCAMENTO 12
#define TAMANHO_TLB 16
#define QUADROS_MAXIMOS 256
#define ENTRADAS_TABELA_PAGINAS 32

using namespace std;

// Entrada da Tabela de Páginas
struct EntradaTabelaPagina {
    bool valido = false;
    bool acessado = false;
    bool sujo = false;
    int numeroQuadro = -1;
};

// Entrada da TLB com algoritmo Segunda Chance
struct EntradaTLBSegundaChance {
    uint32_t paginaVirtual;
    int quadro;
    bool bitUso;
};


// ---------------------------
// Classe da TLB
// ---------------------------

class TLB {
private:
    vector<EntradaTLBSegundaChance> entradas;
    int ponteiro = 0; // Ponteiro circular para o algoritmo de substituição

public:
    // Verifica se a página está na TLB
    bool obter(uint32_t paginaVirtual, int &quadro) {
        for (auto &entrada : entradas) {
            if (entrada.paginaVirtual == paginaVirtual) {
                quadro = entrada.quadro;
                entrada.bitUso = true; // Marca como usada
                return true;
            }
        }
        return false;
    }

    // Insere uma nova entrada na TLB, utilizando Segunda Chance
    void inserir(uint32_t paginaVirtual, int quadro) {
        for (auto &entrada : entradas) {
            if (entrada.paginaVirtual == paginaVirtual) {
                entrada.quadro = quadro;
                entrada.bitUso = true;
                return;
            }
        }

         // Se houver espaço livre, adiciona
        if (entradas.size() < TAMANHO_TLB) {
            entradas.push_back({paginaVirtual, quadro, true});
            return;
        }

        // Substituição usando Segunda Chance
        while (true) {
            if (!entradas[ponteiro].bitUso) {
                entradas[ponteiro] = {paginaVirtual, quadro, true};
                ponteiro = (ponteiro + 1) % TAMANHO_TLB;
                break;
            } else {
                entradas[ponteiro].bitUso = false;
                ponteiro = (ponteiro + 1) % TAMANHO_TLB;
            }
        }
    }
};


// ---------------------------
// Funções Auxiliares
// ---------------------------

// Carrega os dados da memória física a partir de um arquivo
vector<int> carregarMemoriaFisica(const string &caminho) {
    ifstream arquivo(caminho);
    vector<int> memoria;
    int valor;
    while (arquivo >> valor) {
        memoria.push_back(valor);
    }
    return memoria;
}

// Simula o carregamento da página da backing store
int carregarDaBackingStore(uint32_t numeroPagina) {
    ifstream arquivo("backing_store.txt");
    int valor;
    for (uint32_t i = 0; i <= numeroPagina; ++i) {
        if (!(arquivo >> valor)) {
            cerr << "Erro: backing_store.txt nao possui a pagina " << numeroPagina << endl;
            exit(1);
        }
    }
    return valor;
}

// Aloca um quadro de memória física para uma página
int alocarQuadro(vector<int> &memoriaFisica, uint32_t numeroPagina, int valorPagina) {
    int quadro = numeroPagina % QUADROS_MAXIMOS;
    int enderecoBase = quadro * TAMANHO_PAGINA;
    memoriaFisica[enderecoBase] = valorPagina;
    return quadro;
}

// Inicializa a tabela de páginas de 16 bits
void inicializarTabelaPaginas(vector<int> &memoriaFisica, vector<EntradaTabelaPagina> &tabelaPaginas) {
    int quadrosUsados = memoriaFisica.size() / TAMANHO_PAGINA;
    for (int i = 0; i < quadrosUsados && i < ENTRADAS_TABELA_PAGINAS; ++i) {
        EntradaTabelaPagina entrada;
        entrada.valido = true;
        entrada.acessado = false;
        entrada.sujo = false;
        entrada.numeroQuadro = i;
        tabelaPaginas[i] = entrada;
    }
}

// Converte o endereço em string (decimal ou hexadecimal) para inteiro
uint32_t converterEndereco(const string &entrada) {
    uint32_t endereco;
    if (entrada.find("0x") == 0 || entrada.find("0X") == 0) {
        stringstream ss;
        ss << hex << entrada;
        ss >> endereco;
    } else {
        endereco = static_cast<uint32_t>(stoul(entrada));
    }
    return endereco;
}

// ---------------------------
// Gerenciamento de Endereços 16 bits
// ---------------------------

void processarEndereco16Bits(uint32_t endereco, vector<int> &memoriaFisica,
                             vector<EntradaTabelaPagina> &tabelaPaginas, TLB &tlb) {
    uint32_t numeroPagina = endereco >> BITS_DESLOCAMENTO;
    uint32_t deslocamento = endereco & (TAMANHO_PAGINA - 1);
    uint32_t paginaVirtual = numeroPagina;

    cout << "Endereco virtual (16 bits): " << endereco << endl;
    cout << "Pagina: " << numeroPagina << " | Deslocamento: " << deslocamento << endl;

    int quadro;
    if (tlb.obter(paginaVirtual, quadro)) {
        cout << "TLB hit\n";
    } else {
        cout << "TLB miss\n";
        EntradaTabelaPagina &entrada = tabelaPaginas[numeroPagina];
        if (!entrada.valido) {
            cout << "Page fault\n";
            int valor = carregarDaBackingStore(numeroPagina);
            quadro = alocarQuadro(memoriaFisica, numeroPagina, valor);
            entrada.valido = true;
            entrada.acessado = true;
            entrada.sujo = false;
            entrada.numeroQuadro = quadro;
            cout << "Carregado da backing store\n";
        } else {
            cout << "Page hit\n";
            quadro = entrada.numeroQuadro;
        }
        tlb.inserir(paginaVirtual, quadro);
    }

    tabelaPaginas[numeroPagina].acessado = true;

    bool escrita = false; 
    if (escrita) {
        tabelaPaginas[numeroPagina].sujo = true;
        cout << "Simulando escrita: bit sujo setado.\n";
    }

    int enderecoFisico = quadro * TAMANHO_PAGINA + deslocamento;
    if (enderecoFisico < memoriaFisica.size()) {
        cout << "Valor lido da memoria fisica: " << memoriaFisica[enderecoFisico] << "\n\n";
    } else {
        cout << "Endereco fisico invalido\n\n";
    }
}

// ---------------------------
// Gerenciamento de Endereços 32 bits (Tabela de 2 Níveis)
// ---------------------------

void processarEndereco32Bits(uint32_t endereco, vector<int> &memoriaFisica,
                             vector<vector<EntradaTabelaPagina>> &tabela2Niveis, TLB &tlb) {
    uint32_t nivel1 = (endereco >> 22) & 0x3FF;
    uint32_t nivel2 = (endereco >> 12) & 0x3FF;
    uint32_t deslocamento = endereco & 0xFFF;
    uint32_t paginaVirtual = (nivel1 << 10) | nivel2;

    cout << "Endereco virtual (32 bits): " << endereco << endl;
    cout << "Nivel 1: " << nivel1 << " | Nivel 2: " << nivel2 << " | Deslocamento: " << deslocamento << endl;

    int quadro;
    if (tlb.obter(paginaVirtual, quadro)) {
        cout << "TLB hit\n";
    } else {
        cout << "TLB miss\n";

        if (tabela2Niveis[nivel1].empty()) {
            tabela2Niveis[nivel1] = vector<EntradaTabelaPagina>(1024);
        }

        EntradaTabelaPagina &entrada = tabela2Niveis[nivel1][nivel2];
        if (!entrada.valido) {
            cout << "Page fault\n";
            int valor = carregarDaBackingStore(paginaVirtual);
            quadro = alocarQuadro(memoriaFisica, paginaVirtual, valor);
            entrada.valido = true;
            entrada.acessado = true;
            entrada.sujo = false;
            entrada.numeroQuadro = quadro;
            cout << "Carregado da backing store\n";
        } else {
            cout << "Page hit\n";
            quadro = entrada.numeroQuadro;
        }

        tlb.inserir(paginaVirtual, quadro);
    }

    tabela2Niveis[nivel1][nivel2].acessado = true;

    bool escrita = false;
    if (escrita) {
        tabela2Niveis[nivel1][nivel2].sujo = true;
        cout << "Simulando escrita: bit sujo setado.\n";
    }

    int enderecoFisico = quadro * TAMANHO_PAGINA + deslocamento;
    if (enderecoFisico < memoriaFisica.size()) {
        cout << "Valor lido da memoria fisica: " << memoriaFisica[enderecoFisico] << "\n\n";
    } else {
        cout << "Endereco fisico invalido\n\n";
    }
}

// ---------------------------
// Função Principal
// ---------------------------

int main(int argc, char *argv[]) {
    string arquivoEntrada = "addresses.txt";
    if (argc == 2) arquivoEntrada = argv[1];

    ifstream entrada(arquivoEntrada);
    if (!entrada) {
        cerr << "Erro ao abrir o arquivo: " << arquivoEntrada << endl;
        return 1;
    }

    // Inicializa memória e tabelas
    vector<int> memoriaFisica = carregarMemoriaFisica("data_memory.txt");
    vector<EntradaTabelaPagina> tabelaPaginas16(ENTRADAS_TABELA_PAGINAS);
    vector<vector<EntradaTabelaPagina>> tabelaPaginas32(1024);
    TLB tlb;

    inicializarTabelaPaginas(memoriaFisica, tabelaPaginas16);

    // Processa os endereços do arquivo
    string linha;
    while (getline(entrada, linha)) {
        if (!linha.empty()) {
            uint32_t endereco = converterEndereco(linha);
            if (endereco < (1U << 16)) {
                processarEndereco16Bits(endereco, memoriaFisica, tabelaPaginas16, tlb);
            } else {
                processarEndereco32Bits(endereco, memoriaFisica, tabelaPaginas32, tlb);
            }
        }
    }

    return 0;
}
