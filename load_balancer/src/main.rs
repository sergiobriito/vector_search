use glommio::net::{TcpListener, TcpStream};
use glommio::{LocalExecutorBuilder, Placement};
use futures_lite::{io::copy, AsyncReadExt, AsyncWriteExt}; // AsyncReadExt is required for .split()
use std::sync::Arc;
use std::sync::atomic::{AtomicUsize, Ordering};

struct BackendPool {
    backends: Vec<String>,
    idx: AtomicUsize,
}

impl BackendPool {
    fn new(backends: Vec<String>) -> Self {
        Self {
            backends,
            idx: AtomicUsize::new(0),
        }
    }

    fn next(&self) -> &str {
        let i = self.idx.fetch_add(1, Ordering::Relaxed);
        &self.backends[i % self.backends.len()]
    }
}

async fn handle_client(client: TcpStream, pool: Arc<BackendPool>) {
    let backend_addr = pool.next().to_string();

    let backend = match TcpStream::connect(&backend_addr).await {
        Ok(s) => s,
        Err(_) => return,
    };

    // Correct way to bifurcate the stream in Glommio 0.9.0
    // This splits the stream into an owned Read half and an owned Write half
    let (mut client_reader, mut client_writer) = client.split();
    let (mut backend_reader, mut backend_writer) = backend.split();

    // Task 1: Client -> Backend (Requests)
    let t1 = glommio::spawn_local(async move {
        let _ = copy(&mut client_reader, &mut backend_writer).await;
        let _ = backend_writer.close().await;
    });

    // Task 2: Backend -> Client (Responses)
    let t2 = glommio::spawn_local(async move {
        let _ = copy(&mut backend_reader, &mut client_writer).await;
        let _ = client_writer.close().await;
    });

    // Join the tasks to keep the connection alive until both finish
    let _ = futures_lite::future::zip(t1, t2).await;
}

fn main() {
    // 1 CPU unit limit: Placement::Unbound allows the OS to schedule 
    // the single thread efficiently. 
    // ring_depth(128) keeps the memory footprint very low.
    let executor = LocalExecutorBuilder::new(Placement::Unbound)
        .ring_depth(128)
        .spawn(|| async move {
            let addr = "0.0.0.0:8080";
            let listener = TcpListener::bind(addr).expect("Bind failed");
            println!("Glommio LB active on {}", addr);

            let pool = Arc::new(BackendPool::new(vec![
                "backend1:8001".into(),
                "backend2:8002".into(),
            ]));

            loop {
                if let Ok(client) = listener.accept().await {
                    let pool = pool.clone();
                    glommio::spawn_local(handle_client(client, pool)).detach();
                }
            }
        })
        .expect("Executor failed");

    executor.join().unwrap();
}